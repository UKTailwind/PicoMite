"""make.py: builds apps/scummvm/out/scummvm.app, ScummVM's SCUMM engine (the LucasArts adventures) for the PicoMite (the
PC3), from the Colour Maximite 2's port of frank-quest (rh1tech's port of Cabal, a fork of ScummVM, to the RP2350).
frank-quest is fetched the first time into apps/scummvm/frank-quest (git ignores it) at the commit below, and ScummVM's
core and the SCUMM engines (v1-v6, and v7 for Full Throttle) are built unaltered from the source lists in its
CMakeLists.txt. The other engines (AGI, SCI, GOB, KYRA) are left out, to leave the PSRAM to the games.

What touches the RP2350 is replaced by a file of the same path in overlay/, as on the CMM2: the display (MODE 5,
320x240 in 256 colours, 320x200 letterboxed), the input, the sound mixer's connection to the stream, the files, and the
PSRAM heap, on the loader's services. stub/ holds the few Pico SDK headers the engines include. Two more overlay files
keep the speech off the SD card while the sound is made, as the sound stream's fill runs in an interrupt and may call no
service: audio/decoders/voc.cpp reads each line of speech into memory before it plays, and frank_quest_fs.c refuses
a read made in an interrupt.

The program is GPL-2.0-or-later, as ScummVM is: its source must go with any copy given out (frank-quest's, at that
commit, and this directory).

    python apps/scummvm/make.py [-j JOBS]
then put out/scummvm.app in B:/quest, the games in B:/quest/<dir>/ (dott, samnmax, ...: frank-quest's README lists
the names), and at the prompt:
    DRIVE "B:" : CHDIR "/quest"
    RUN "scummvm.app"                    (or RUN "scummvm.app", "dott": that folder's game at once)
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, 'frank-quest')
URL = 'https://github.com/rh1tech/frank-quest.git'
COMMIT = '949301b'                                       # v1.00, 12 May 2026

# frank-quest's own files, and its drivers that are kept (the allocator, on the loader's heap)
MAIN = ['src/main.c', 'src/frank_quest_main.cpp', 'src/frank_quest_selector.cpp', 'src/frank_quest_welcome.cpp',
        'src/frank_quest_new.cpp', 'src/frank_quest_fs.c', 'src/frank_quest_sbrk.c', 'src/frank_quest_savemenu.cpp',
        'drivers/psram_allocator.c', 'drivers/dlmalloc_cmm2.c']
SETS = ['COMMON', 'GRAPHICS', 'GUI', 'VIDEO', 'IMAGE', 'AUDIO', 'BASE', 'ENGINE', 'SCUMM_ENGINE', 'SCUMM_7_8',
        'BACKEND']
DEFINES = ['BOARD_M2', 'CPU_CLOCK_MHZ=315', 'CABAL_PSRAM_SIZE_BYTES=8388608', 'CABAL_VERSION="1.00-picomite"',
           'CABAL_SCREEN_WIDTH=320', 'CABAL_SCREEN_HEIGHT=200', 'CABAL_HDMI_HEIGHT=240',
           'ENABLE_SCUMM=1', 'ENABLE_SCUMM_7_8=1',
           'CABAL_LOW_MEMORY=1', 'PICO_MALLOC_PANIC=0', 'SKIP_PICO_MALLOC=1', 'PICO_ON_DEVICE=1', '__RP2350__=1',
           'CABAL_HAVE_STDINT_H=1', 'DISABLE_GUI=1', 'DISABLE_DEFAULT_AUDIOCD_MANAGER=1', 'CABAL_MINIMAL_AUDIO=1',
           'USE_I2S_AUDIO=1', 'CMM2=1', 'PICOMITE=1',
           'NDEBUG']                                     # (frank-quest's builds are the Pico SDK's Release: no asserts)


def sources():
    """the files frank-quest's CMakeLists.txt names in its *_SOURCES sets (one a line, comments after #)"""
    lists, cur = {}, None
    for line in open(os.path.join(SRC, 'CMakeLists.txt')):
        m = re.match(r'\s*set\((\w+)_SOURCES\s*$', line)
        if m:
            cur = lists.setdefault(m.group(1), [])
            continue
        if cur is not None:
            if line.strip() == ')':
                cur = None
                continue
            w = line.split('#', 1)[0].strip()
            if w.endswith(('.c', '.cpp')):
                cur.append(w)
    missing = [s for s in SETS if s not in lists]
    if missing:
        sys.exit('CMakeLists.txt has no %s' % ', '.join(missing))
    return MAIN + [f for s in SETS for f in lists[s]]


def main():
    if not os.path.isdir(SRC):
        subprocess.run(['git', 'clone', '-q', URL, SRC], check=True)
        subprocess.run(['git', '-C', SRC, 'checkout', '-q', COMMIT], check=True)
    head = subprocess.run(['git', '-C', SRC, 'rev-parse', '--short=7', 'HEAD'], capture_output=True, text=True).stdout
    if head.strip() != COMMIT:
        sys.exit('%s is at %s, not %s' % (SRC, head.strip(), COMMIT))
    out = os.path.join(HERE, 'out')
    os.makedirs(out, exist_ok=True)
    files, seen = [], set()
    for f in sources():
        if f in seen:
            continue
        seen.add(f)
        o = os.path.join(HERE, 'overlay', f)
        files.append(o if os.path.exists(o) else os.path.join(SRC, f))
    listfile = os.path.join(out, 'sources.txt')
    open(listfile, 'w').write('\n'.join(files) + '\n')
    inc = [os.path.join(HERE, 'overlay', 'src'), os.path.join(HERE, 'overlay', 'drivers'), os.path.join(SRC, 'src'),
           os.path.join(SRC, 'src', 'engines'), os.path.join(SRC, 'drivers'), os.path.join(HERE, 'stub')]
    cmd = [sys.executable, '-I', os.path.join(REPO, 'tools', 'sdk', 'build.py'), os.path.join(out, 'scummvm.app'),
           '--sources', listfile, '--api', '1', '--psram', '--stack', '0x10000']
    for i in inc:
        cmd += ['-I', i]
    cmd += ['--cflag=-D' + d for d in DEFINES] + ['--cflag=-w']
    cmd += ['--ldflag=-Wl,--wrap=' + f for f in ('malloc', 'calloc', 'realloc', 'free')]
    cmd += sys.argv[1:]
    sys.exit(subprocess.run(cmd).returncode)


if __name__ == '__main__':
    main()
