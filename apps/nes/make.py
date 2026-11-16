"""make.py: builds apps/nes/out/nes.app, a NES for the PicoMite (the PC3), from the Colour Maximite 2's port: frank-nes (rh1tech's RP2350 NES, on Shay Green's QuickNES core), fetched
the first time into apps/nes/frank-nes (git ignores it) at the commit below, patches/*.patch applied to it in order. Its core, ROM
selector, settings and main loop are built as they are, the build the RP2350's soft framebuffer (HDMI_PIO): an 8-bit 256 x 240 picture
and a palette, which is what the CMM2's MODE 7,8 is. stub/ stands in for the Pico SDK and for FatFs, nes_cmm2.c for the drivers (the
display, the sound stream, the USB gamepads and keyboard, on the loader's services) and ff_cmm2.c for the files (Peter, 5 Nov 2026).

The program is GPL-3.0, as frank-nes is: its source must go with any copy given out (frank-nes's, at that commit, and this directory).

Built --vram, so that a G1 runs it too. MODE 7,8 is the mode a G1 keeps while it lends its display memory.

    python apps/nes/make.py
then put out/nes.app and the ROMs in A:/nes, and at the CMM2's prompt:
    CHDIR "A:/nes"
    RUN "nes.app"
"""
import glob, hashlib, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, 'frank-nes')
URL = 'https://github.com/rh1tech/frank-nes.git'
COMMIT = '7e6d0d9ec4500e246bc489cbc8eea0877c030040'   # 12 May 2026

DEFINES = ['HDMI_PIO', 'USB_HID_ENABLED=1', 'HAS_NESPAD=1', 'NDEBUG', 'NO_UNALIGNED_ACCESS', 'QNES_EXT_TILE_CACHE',
           'CPU_CLOCK_MHZ=480', 'FLASH_CLOCK_MHZ=100', 'CPU_VOLTAGE=0', 'FRANK_NES_VERSION="1.2"']


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
    """QNES_SOURCES and EMU2413_SOURCES of its CMakeLists.txt, and the platform's files but for the drivers"""
    core = os.path.join(SRC, 'src', 'core_quicknes')
    plat = os.path.join(SRC, 'src', 'platform', 'pico')
    out = glob.glob(os.path.join(core, '*.cpp')) + glob.glob(os.path.join(core, 'mappers', '*.cpp'))
    out += glob.glob(os.path.join(SRC, 'src', 'emu2413', '*.cpp'))
    out += [os.path.join(plat, f) for f in ('main_pico.c', 'rom_selector.c', 'settings.c', 'palettes.c')]
    out += [os.path.join(HERE, 'nes_pico.c'), os.path.join(HERE, 'ff_cmm2.c')]
    return sorted(out)


def main():
    if not os.path.isdir(SRC):
        subprocess.run(['git', 'clone', '-q', URL, SRC], check=True)
        subprocess.run(['git', '-C', SRC, 'checkout', '-q', COMMIT], check=True)
    head = subprocess.run(['git', '-C', SRC, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    if head != COMMIT:
        sys.exit('%s is at %s, not %s' % (SRC, head, COMMIT))
    patch()
    os.makedirs(os.path.join(HERE, 'out'), exist_ok=True)
    listfile = os.path.join(HERE, 'out', 'sources.txt')
    open(listfile, 'w').write('\n'.join(sources()) + '\n')
    inc = [os.path.join(HERE, 'stub'), os.path.join(SRC, 'src', 'core_quicknes'), os.path.join(SRC, 'src', 'emu2413'),
           os.path.join(SRC, 'src', 'platform', 'pico'), os.path.join(SRC, 'drivers')]
    inc += [os.path.join(SRC, 'drivers', d) for d in ('usbhid', 'nespad', 'ps2kbd', 'i2s', 'pwm_audio')]
    cmd = [sys.executable, '-I', os.path.join(REPO, 'tools', 'sdk', 'build.py'), os.path.join(HERE, 'out', 'nes.app'),
           '--sources', listfile, '--api', '1', '--psram', '--stack', '0x10000', '--heap', str((4 << 20) + (512 << 10))]
    for d in inc:
        cmd += ['-I', d]
    cmd += ['--cflag=-D' + d for d in DEFINES] + ['--cflag=-w']
    cmd += ['--cflag=' + a for a in sys.argv[1:] if a.startswith('-')]
    sys.exit(subprocess.run(cmd).returncode)


if __name__ == '__main__':
    main()
