"""make.py: builds apps/bbc/out/bbc.app, a BBC Micro (Model B and Master 128) for the PicoMite (the PC3), from the Colour Maximite 2's port
(its apps/bbc, whose patches/, stub/ and ff_cmm2.c are here unchanged): frank-micro (rh1tech's RP2350 port of Tom Walker's B-em, by
way of Graham Sanderson's Pico B-em), fetched the first time into apps/bbc/frank-micro (git ignores it) at the commit below, patches/*.patch applied to it
in order (Peter, 11 Nov 2026: "There is a frank-bbc that might be better as a true emulator"). B-em's emulator, with Graham Sanderson's Thumb 6502 and
event scheduler as frank-micro builds them (in their PI_ASM32 form, which needs no RP2350 interpolator, assembled as Thumb-2: patches/0001), and
frank-micro's keyboard, disc loader, settings and on-screen menus are built as they are, but for its frank_platform.c (the RP2350's start-up), frank_gui.c (its HDMI picture) and frank_audio.c (its HDMI, I2S and PWM sound): bbc_pico.c has
what the rest needs of those, and stands in for the drivers (the display, the sound stream, the USB keyboard and gamepads, on the loader's services),
ff_cmm2.c for FatFs, and stub/ for the Pico SDK.

The program is GPL-3.0, as frank-micro is (B-em GPL-2.0 or later): its source must go with any copy given out (frank-micro's, at that commit, and this
directory). The BBC's ROMs (the MOS, BASIC, DFS and the Master's) are frank-micro's, built in (Peter, 11 Nov 2026: "built-in").

The patches (the CMM2's: CMM2_APP is defined here too), in order: 0001 the CMM2 build (the Thumb 6502 as Thumb-2, the screenshot's size, a model change without a reboot), 0002 disc writing,
0003 the ISO keyboard's two keys, 0004 the Model B's sideways RAM and the 6502's time kept across a model change, 0005 the Model B's start mode.

    python apps/bbc/make.py
then put out/bbc.app in B:/bbc, the disc images (.ssd, .dsd, .adf, .adl, .img) in B:/bbc/disk and any sideways ROM images (.rom, .bin) in
B:/bbc/roms (frank-micro's /micro is this folder), and at the prompt:
    DRIVE "B:" : CHDIR "/bbc"
    RUN "bbc.app"
"""
import glob, hashlib, json, os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, 'frank-micro')
URL = 'https://github.com/rh1tech/frank-micro.git'
COMMIT = 'c27ffbfd890ba5a3ec9b72b697562d6e01423a02'
SRAM_DEFAULT = ['asm', 'display', 'bemdisplay']   # (measured at 576p: see main)
CPU = ['-mcpu=cortex-m33', '-mthumb', '-mfloat-abi=softfp', '-mfpu=fpv5-sp-d16']   # (the firmware's, as tools/sdk/build.py's)

# its CMakeLists.txt's, for the M2 board (its pins are not used here), with its Thumb 6502 and event scheduler (USE_THUMB_CPU) in the form
# B-em Pico's PI_ASM32 has it (the page's handler by a register, not the RP2350's interpolator: PI_ASM32 and gen_asm.rb's PI_ASM), USB HID on
# (the keyboard and the gamepads come by it: bbc_cmm2.c), and the 640 x 256 picture its screenshots are taken of (bbc_cmm2.c)
DEFINES = ['CMM2_APP', 'BOARD_M2', 'PLATFORM_M2=1', 'PICO_BUILD=1', 'PICO_ON_DEVICE=1', 'FRANK_PERF_TIMING_HACKS=1', 'LSB_FIRST=1', 'HDMI_PIO=1',
           'USB_HID_ENABLED=1', 'FRANK_MICRO_VERSION="1.00"', 'BEM', 'USE_MEMORY_POINTER', 'HAVE_STPCPY', 'VERSION="frank-micro"', 'X_GUI',
           'SINGLE_CORE', 'FRANK_MAIN', 'MODEL_MASTER', 'NO_USE_KEYCODE_MAP',
           'NO_USE_TUBE', 'NO_USE_DEBUGGER', 'NO_USE_SID', 'NO_USE_SAVE_STATE', 'NO_USE_VDFS', 'NO_USE_UEF', 'NO_USE_CSW', 'NO_USE_MMB', 'NO_USE_IDE',
           'NO_USE_ADC', 'NO_USE_MOUSE', 'NO_USE_MUSIC5000', 'NO_USE_SCSI', 'NO_USE_FDI', 'NO_USE_I8271', 'NO_USE_ALLEGRO_GUI', 'NO_USE_JOYSTICK',
           'NO_USE_PAL', 'NO_USE_COMPACT', 'NO_USE_CMD_LINE', 'NO_USE_SOUND_FILTER', 'NO_USE_SET_SPEED', 'NO_USE_NULA_ATTRIBUTE',
           'NO_USE_NULA_PIXEL_HSCROLL', 'NO_USE_DD_NOISE', 'NO_USE_WRITABLE_CONFIG', 'NO_USE_CLOSE', 'NO_USE_CMOS_SAVE', 'NO_USE_RAM_ROMS',
           'NO_USE_MIDI', 'TILE8', 'MODE7_TILE8', 'USE_CRTC0_0_OPTIMIZATION', 'LOOK_MA_NO_MA', 'USE_MEM_DDNOISE', 'USE_DDNOISE_8',
           'NO_USE_KEY_LOOKUP', 'NO_USE_DISC_WRITE', 'SHORTCIRCUIT_KEYBOARD_POLL', 'USE_SECTOR_READ',
           'PIOC_PRINTF_SUPPORT_FLOAT=0', 'PIOC_PRINTF_SUPPORT_LONG_LONG=0', 'MICRO_FB_WIDTH=640', 'MICRO_FB_HEIGHT=256',
           'USE_PICO_CPU', 'USE_HW_EVENT', 'THUMB_CPU_BEEB', 'THUMB_CPU_USE_ASM=1', 'CPU_ASM_INLINE_BREAKOUT', 'PI_ASM32=1']

ENGINE = ['acia', 'adc', 'cmos', 'compactcmos', 'compact_joystick', 'compat_wrappers', 'config', 'csw', 'ddnoise', 'disc', 'fdi', 'fdi2raw',
          'i8271', 'ide', 'joystick', 'keyboard', 'logging', 'main', 'mem', 'model', 'mouse', 'music2000', 'music4000', 'music5000', 'pal', 'scsi',
          'sdf-acc', 'sdf-geo', 'serial', 'sn76489', 'sysacia', 'sysvia', 'tape', 'tapenoise', 'uef', 'uservia', 'via', 'wd1770', 'win']
PORT = ['frank_keyboard','frank_stubs', 'frank_disc', 'frank_console', 'frank_settings', 'frank_loader', 'frank_ui', 'frank_screenshot']


def patch():
    """the patches applied to the commit's files, once: again (from the commit) only when they change"""
    patches = sorted(glob.glob(os.path.join(HERE, 'patches', '*.patch')))
    want = hashlib.sha1(b''.join(open(p, 'rb').read() for p in patches)).hexdigest()
    stamp = os.path.join(SRC, '.cmm2-patches')
    if os.path.exists(stamp) and open(stamp).read().strip() == want:
        return
    subprocess.run(['git', '-C', SRC, 'checkout', '-q', '--', '.'], check=True)
    for p in patches:
        subprocess.run(['git', '-C', SRC, 'apply', p], check=True)
    open(stamp, 'w').write(want + '\n')


def sources():
    """its CMakeLists.txt's BBC_ENGINE and PICO_PORT, but for frank_platform.c, frank_gui.c and frank_audio.c"""
    bem = os.path.join(SRC, 'src', 'bem')
    out = [os.path.join(bem, f + '.c') for f in ENGINE]
    out += [os.path.join(bem, 'pico', p) for p in ('video/video.c', 'video/display.c', 'video/gui_stub.c', 'audio/audio.c',
                                                    'sector_read/sector_read.c', 'stub_allegro5/al_stub.cpp', 'roms/embedded_roms.c')]
    out += [os.path.join(bem, 'thumb_cpu', 'src', f) for f in ('cpu/cpu.c', 'cpu_mem.c', 'adapter.c')]
    out += [os.path.join(SRC, 'src', 'frank', f + '.c') for f in PORT]
    out += [os.path.join(SRC, 'src', 'Pico', f + '.c') for f in ('ui_draw', 'ui_font')]
    out += [os.path.join(HERE, f) for f in ('bbc_pico.c', 'ff_cmm2.c')]
    return sorted(out)


def gcc():
    """the firmware's arm-none-eabi-gcc, found as tools/sdk/build.py finds it: ARM_TOOLCHAIN_DIR, the PATH's, the Arm GNU
    Toolchain 13.3's usual folder"""
    exe = 'arm-none-eabi-gcc' + ('.exe' if os.name == 'nt' else '')
    for d in (os.environ.get('ARM_TOOLCHAIN_DIR', ''), os.path.dirname(shutil.which('arm-none-eabi-gcc') or ''),
              os.path.join('C:' + os.sep, 'Program Files (x86)', 'Arm GNU Toolchain arm-none-eabi', '13.3 rel1', 'bin')):
        if d and os.path.exists(os.path.join(d, exe)):
            return os.path.join(d, exe)
    sys.exit('arm-none-eabi-gcc not found: set ARM_TOOLCHAIN_DIR to its folder, or put it on the PATH')


def thumb_gen():
    """the Thumb 6502's tables (6502_asm.inl, 6502_c.inl) by gen_asm.rb with PI_ASM and THUMB2 (patches/: a handler's C function called at its own odd address), as its CMakeLists.txt makes them (in WSL's Ruby, or the
    PATH's), each handler then marked a Thumb function (.thumb_func: PI_ASM's are ARM code), so that its jump table's addresses are odd"""
    gen = os.path.join(SRC, 'src', 'bem', 'thumb_cpu')
    rb = os.path.join(gen, 'gen', 'gen_asm.rb')
    out = os.path.join(HERE, 'out', 'thumb_gen')
    asm, c = os.path.join(out, '6502_asm.inl'), os.path.join(out, '6502_c.inl')
    if os.path.exists(asm) and os.path.exists(c) and os.path.getmtime(asm) > os.path.getmtime(rb):
        return out
    os.makedirs(out, exist_ok=True)
    raw = os.path.join(tempfile.gettempdir(), 'bbc_6502_asm_raw.inl')   # (outside Dropbox, which can hold a new file open a moment)

    def wsl(path):
        d, rest = os.path.splitdrive(os.path.abspath(path))
        return '/mnt/' + d[0].lower() + rest.replace(chr(92), '/')
    if shutil.which('ruby'):
        subprocess.run(['ruby', 'gen/gen_asm.rb', raw, c, '1', '1'], cwd=gen, check=True, capture_output=True)
    else:
        subprocess.run(['wsl', '-e', 'bash', '-c', 'cd "%s" && ruby gen/gen_asm.rb "%s" "%s" 1 1' % (wsl(gen), wsl(raw), wsl(c))], check=True,
                       capture_output=True)
    lines = []
    for l in open(raw, encoding='latin-1').read().split(chr(10)):
        lines.append(l)
        if l.startswith('.global '):
            lines.append('.thumb_func')
    open(asm, 'w', encoding='latin-1', newline=chr(10)).write(chr(10).join(lines))
    os.remove(raw)
    return out


def cpu_asm_o(gen_dir):
    """cpu_asm.S assembled for the M33 as Thumb-2 (PI_ASM32, CMM2_APP: patches/)"""
    src = os.path.join(SRC, 'src', 'bem', 'thumb_cpu', 'src', 'cpu', 'cpu_asm.S')
    obj = os.path.join(HERE, 'out', 'cpu_asm_00000000.o')
    newest = max(os.path.getmtime(f) for f in (src, os.path.join(gen_dir, '6502_asm.inl'), __file__))
    if os.path.exists(obj) and os.path.getmtime(obj) > newest:
        return obj
    bem = os.path.join(SRC, 'src', 'bem')
    cmd = [gcc(), '-c'] + CPU + ['-o', obj, src,
           '-I', gen_dir, '-I', os.path.join(bem, 'thumb_cpu', 'src'), '-I', os.path.join(bem, 'pico', 'video'), '-I', bem,
           '-I', os.path.join(bem, 'pico')]
    cmd += ['-D' + d for d in ('CMM2_APP', 'MODEL_MASTER', 'PI_ASM32=1', 'THUMB_CPU_BEEB', 'THUMB_CPU_USE_ASM=1', 'CPU_ASM_INLINE_BREAKOUT',
                               'USE_HW_EVENT', 'USE_PICO_CPU')]
    subprocess.run(cmd, check=True)
    return obj


def pixels_o():
    """pico/video/pixels.S (display.c's draw_pixels8x2x2) assembled for the M33: build.py takes C, C++ and objects"""
    src = os.path.join(SRC, 'src', 'bem', 'pico', 'video', 'pixels.S')
    obj = os.path.join(HERE, 'out', 'pixels.o')
    if not os.path.exists(obj) or os.path.getmtime(obj) < os.path.getmtime(src):
        subprocess.run([gcc(), '-c'] + CPU + ['-DCMM2_APP', '-I', os.path.dirname(src),
                        '-I', os.path.join(SRC, 'src', 'bem', 'pico'), '-o', obj, src], check=True)
    return obj


def main():
    if not os.path.isdir(SRC):                  # (that commit alone: the whole history will not unpack in a Dropbox folder)
        subprocess.run(['git', 'init', '-q', SRC], check=True)
        subprocess.run(['git', '-C', SRC, 'fetch', '-q', '--depth', '1', URL, COMMIT], check=True)
        subprocess.run(['git', '-C', SRC, 'checkout', '-q', 'FETCH_HEAD'], check=True)
    head = subprocess.run(['git', '-C', SRC, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    if head != COMMIT:                          # (first: in a folder with a broken .git, git -C falls through to the CMM2 repository)
        sys.exit('%s is at %s, not %s' % (SRC, head, COMMIT))
    patch()
    os.makedirs(os.path.join(HERE, 'out'), exist_ok=True)
    listfile = os.path.join(HERE, 'out', 'sources.txt')
    gen_dir = thumb_gen()
    open(listfile, 'w').write('\n'.join(sources() + [pixels_o(), cpu_asm_o(gen_dir)]) + '\n')
    s = os.path.join(SRC, 'src')
    inc = [os.path.join(HERE, 'stub'), HERE, s, os.path.join(s, 'Pico'), os.path.join(s, 'frank'), os.path.join(s, 'frank', 'vendor'),
           os.path.join(s, 'bem'), os.path.join(s, 'bem', 'pico'), os.path.join(s, 'bem', 'pico', 'video'), os.path.join(s, 'bem', 'pico', 'audio'),
           os.path.join(s, 'bem', 'pico', 'x_gui'), os.path.join(s, 'bem', 'pico', 'stub_allegro5', 'include'), os.path.join(s, 'bem', 'pico', 'roms'),
           os.path.join(s, 'bem', 'pico', 'sector_read'), os.path.join(s, 'bem', 'thumb_cpu', 'src'), gen_dir, os.path.join(SRC, 'drivers'), os.path.join(SRC, 'drivers', 'nespad')]
    cmd = [sys.executable, '-I', os.path.join(REPO, 'tools', 'sdk', 'build.py'), os.path.join(HERE, 'out', 'bbc.app'),
           '--sources', listfile, '--api', '1', '--psram', '--stack', '0x1800', '--heap', str(2 << 20)]
    # (the stack: B-em, the menus and the disc browser reach 1,952 bytes at the most: -stats' high-water mark; 6 KB)
    # the SRAM part (the PSRAM's 16 KB XIP cache cannot hold the hot code, which shares it with the BBC's memory), by
    # sram=asm,frank,display,bemdisplay,cpumem,video (the default: SRAM_DEFAULT): asm the Thumb 6502, its event scheduler
    # and the pixel code; frank the functions frank-micro keeps in RAM (stub/pico/stdlib.h); display bbc_pico.c's display
    # code; bemdisplay, cpumem and video those files of B-em's whole. The line buffers are in it always. At 576p the
    # SRAM heap is 55.5 KB, which the SRAM part shares with the stack, the sound's 8 KB and BASIC's 4.5 KB
    parts = SRAM_DEFAULT
    for a in sys.argv[1:]:
        if a.startswith('sram='):
            parts = [p for p in a[5:].split(',') if p]
    if 'asm' in parts:
        cmd += ['--sram', pixels_o(), '--sram', cpu_asm_o(gen_dir)]
    if 'frank' in parts:
        cmd += ['--cflag=-DBBC_SRAM_FRANK']
    if 'display' in parts:
        cmd += ['--cflag=-DBBC_SRAM_DISPLAY']
    for part, f in (('bemdisplay', 'pico/video/display.c'), ('cpumem', 'thumb_cpu/src/cpu_mem.c'), ('video', 'pico/video/video.c')):
        if part in parts:
            cmd += ['--sram', os.path.join(SRC, 'src', 'bem', *f.split('/'))]
    print('the SRAM part: ' + (', '.join(parts) or 'the line buffers alone'), flush=True)
    for d in inc:
        cmd += ['-I', d]
    # -fwrapv: the Thumb 6502's event times (cpu_mem.c's cycle_timestamp_t) are signed 32-bit counts of the BBC's 2 MHz cycles, compared
    # wrap-safe as (a - b) > 0; without it GCC, taking a signed overflow as impossible, makes that a > b, and when the count passes
    # 0x7FFFFFFF, 17.9 minutes after the start (2^31 / 2 MHz), events fire early, an assert fails (via.c's t1_reached) and the program
    # ends with the screen still in its 576p (a user's G1 report, 13 Nov 2026: gibberish at the top of the screen after up to an hour at
    # the BBC's prompt). Asserts off, as frank-micro's own build has them, the emulation freezes there instead (rh1tech/frank-micro#6)
    cmd += ['--cflag=-D' + d for d in DEFINES] + ['--cflag=-w', '--cflag=-O3', '--cflag=-fno-strict-aliasing', '--cflag=-fwrapv']
    cmd += ['--cflag=' + a for a in sys.argv[1:] if a.startswith('-')]
    sys.exit(subprocess.run(cmd).returncode)


if __name__ == '__main__':
    main()
