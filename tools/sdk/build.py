"""build.py OUT.app SOURCE.c|.cpp|.o [...] [--base ADDR] [--stack BYTES] [--heap BYTES] [--api N] [--psram]
            [--module] [--user] [--no-printf-float] [--libc nano|full] [--cflag FLAG ...] [--cxxflag FLAG ...] [--ldflag FLAG ...]
            [-I DIR ...] [-j JOBS] [--sources FILE]

Builds a loadable program for the PicoMite (RP2350) with the firmware's own toolchain (the folder in
ARM_TOOLCHAIN_DIR, else the PATH's, else the Arm GNU Toolchain 13.3's usual folder): compiles the sources with crt0.c
and syscalls.c for the Cortex-M33 (the firmware's float ABI, softfp: floats and doubles cross the services in the core
registers; C++ as C++14 without exceptions or RTTI, and then linked by g++ for its library), links them with app.ld at
--base (default 0x11000000, the bottom of the PSRAM, where the loader usually puts a program, so nothing is moved), and
makes OUT.app with mkimage.py.
--psram marks the program as needing PSRAM (a board without refuses it).
--module makes a module (PMIMG_KIND_MODULE): crt0.c calls module_main(), which returns its table. --user makes a USER
module (loader/pmuser.h, for USER LOAD): --module, with userlib.c's snprintf, sprintf and printf, which format with
the firmware's own vsnprintf, so the module carries no printf of its own (examples/user/proforma.c is 3 KB).
--no-printf-float leaves out newlib's printf of floats (dtoa, 4 KB), which a program that formats its own does not
need: printf's %f is otherwise always there.
--libc full links the whole newlib instead of newlib-nano (the default).
The ELF and its map are left beside OUT.app. --base 0x11400000 makes a copy that the loader must move (its
relocations are then used): the test of the relocations.

The objects go in OUT's stem + ".obj/", each named by its source's path (engines share file names), and are compiled
JOBS at a time (default: every core). A source is compiled again only when it, or a header it read (gcc -MMD), is
newer than its object, or the flags have changed. --cflag goes to C and C++, --cxxflag to C++ only, --ldflag to the
link. --sources names a file of sources, one a line, for lists too long for a command line. An object (.o) among
the sources is linked as it is.

Do not put the firmware's include folders on -I: Windows ignores case, and its headers (Audio.h...) would be taken
in place of a program's own (audio.h).
"""
import argparse, concurrent.futures, hashlib, json, os, shutil, subprocess, sys

SDK = os.path.dirname(os.path.abspath(__file__))
EXE = '.exe' if os.name == 'nt' else ''
DEFAULT_TOOLCHAIN = r'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\bin'


def toolchain():
    """the folder of arm-none-eabi-gcc: the environment's ARM_TOOLCHAIN_DIR, else wherever the PATH finds it, else
    the Arm GNU Toolchain 13.3's usual folder on Windows"""
    d = os.environ.get('ARM_TOOLCHAIN_DIR', '')
    if d and os.path.exists(os.path.join(d, 'arm-none-eabi-gcc' + EXE)):
        return d
    found = shutil.which('arm-none-eabi-gcc')
    if found:
        return os.path.dirname(found)
    if os.path.exists(os.path.join(DEFAULT_TOOLCHAIN, 'arm-none-eabi-gcc' + EXE)):
        return DEFAULT_TOOLCHAIN
    sys.exit('arm-none-eabi-gcc not found: set ARM_TOOLCHAIN_DIR to its folder, or put it on the PATH')


tooldir = toolchain()
GCC = os.path.join(tooldir, 'arm-none-eabi-gcc' + EXE)
GXX = os.path.join(tooldir, 'arm-none-eabi-g++' + EXE)

NANO = ['--specs=nano.specs']                   # the C library, unless --libc full: newlib-nano's headers and archive
FLAGS = ['-mcpu=cortex-m33', '-mthumb', '-mfloat-abi=softfp', '-mfpu=fpv5-sp-d16',
         '-O2', '-g0', '-Wall', '-ffunction-sections', '-fdata-sections',
         '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-I', SDK]
CFLAGS = FLAGS + ['-std=gnu11']
CXXFLAGS = FLAGS + ['-std=gnu++14', '-fno-rtti', '-fno-threadsafe-statics', '-fno-use-cxa-atexit']


def objname(objdir, src):
    s = os.path.abspath(src)
    return os.path.join(objdir, os.path.splitext(os.path.basename(s))[0] + '_' +
                        hashlib.md5(s.lower().encode()).hexdigest()[:8] + '.o')


def stale(o):
    """True if the object is missing or older than its source or any header its .d names"""
    d = o[:-2] + '.d'
    if not os.path.exists(o) or not os.path.exists(d):
        return True
    t = os.path.getmtime(o)
    deps = open(d).read().replace('\\\n', ' ').split('.o:', 1)[-1]  # (after the target: D: in a path is no end)
    for dep in deps.replace('\\ ', '\0').split():
        dep = dep.replace('\0', ' ')
        if not os.path.exists(dep) or os.path.getmtime(dep) > t:
            return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('sources', nargs='*')
    ap.add_argument('--sources', dest='listfile')
    ap.add_argument('--base', default='0x11000000')
    ap.add_argument('--stack', default='0')
    ap.add_argument('--heap', default='0')
    ap.add_argument('--api', default='1')
    ap.add_argument('--psram', action='store_true')
    ap.add_argument('--fixed', action='store_true')
    ap.add_argument('--module', action='store_true')               # a module (PMIMG_KIND_MODULE): crt0.c's
                                                                    # entry calls module_main(), not main()
    ap.add_argument('--user', action='store_true')                 # a USER module (loader/pmuser.h): --module, and
                                                                    # userlib.c's printf family (the firmware's)
    ap.add_argument('--no-printf-float', dest='pfloat', action='store_false')
    ap.add_argument('--libc', choices=('nano', 'full'), default='nano')
    ap.add_argument('--cflag', action='append', default=[])
    ap.add_argument('--cxxflag', action='append', default=[])
    ap.add_argument('--ldflag', action='append', default=[])
    ap.add_argument('-I', dest='incs', action='append', default=[])
    ap.add_argument('-j', dest='jobs', type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    if a.user:
        a.module = True
        a.pfloat = False                                            # (its printf is the firmware's: userlib.c)
    libc = NANO if a.libc == 'nano' else []                         # (compile and link alike: the headers match the archive)
    if a.module:
        a.cflag += ['-DCMM2_MODULE']
    stem = os.path.splitext(a.out)[0]
    objdir = stem + '.obj'
    os.makedirs(objdir, exist_ok=True)
    incs = ['-I' + i for i in a.incs]
    sources = list(a.sources)
    if a.listfile:
        sources += [l.strip() for l in open(a.listfile) if l.strip() and not l.startswith('#')]
    sources += [os.path.join(SDK, 'crt0.c'), os.path.join(SDK, 'syscalls.c'), os.path.join(SDK, 'fwdouble.S')]
    if a.user:
        sources.append(os.path.join(SDK, 'userlib.c'))
    # the flags as they were: a change rebuilds everything
    key = hashlib.md5(json.dumps([CFLAGS, CXXFLAGS, libc, a.cflag, a.cxxflag, incs]).encode()).hexdigest()
    keyfile = os.path.join(objdir, 'flags.md5')
    same = os.path.exists(keyfile) and open(keyfile).read() == key
    cxx = any(s.lower().endswith(('.cpp', '.cc')) for s in sources)
    jobs, objs = [], []
    for s in sources:
        if s.lower().endswith('.o'):
            objs.append(os.path.abspath(s))
            continue
        # each source to an object by its own compiler (g++ would take every .c after the first as C++, -x c or not)
        isc = not s.lower().endswith(('.cpp', '.cc'))
        o = objname(objdir, s)
        objs.append(o)
        if not same or stale(o):
            cmd = (([GCC] + CFLAGS) if isc else ([GXX] + CXXFLAGS + a.cxxflag)) + libc + a.cflag + incs + \
                  ['-MMD', '-MF', o[:-2] + '.d', '-c', s, '-o', o]
            jobs.append((s, cmd))
    if jobs:
        print('compiling %d of %d sources, %d at a time' % (len(jobs), len(sources), a.jobs), flush=True)
    failed = []
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for (s, cmd), r in zip(jobs, ex.map(lambda j: subprocess.run(j[1], capture_output=True, text=True), jobs)):
            if r.stdout or r.stderr:
                sys.stdout.write(r.stdout + r.stderr)
            if r.returncode:
                failed.append(s)
    if failed:
        sys.exit('%d failed: %s' % (len(failed), ' '.join(os.path.basename(f) for f in failed[:20])))
    open(keyfile, 'w').write(key)
    rsp = stem + '.rsp'
    open(rsp, 'w').write('\n'.join('"%s"' % o.replace('\\', '/') for o in objs) + '\n')
    cmd = ([GXX if cxx else GCC] + FLAGS + libc + a.cflag + ['@' + rsp] +
           ['-nostartfiles', '-T', os.path.join(SDK, 'app.ld'), '-Wl,--defsym=__app_base=' + a.base,
            '-Wl,--emit-relocs', '-Wl,--gc-sections', '-Wl,-Map=' + stem + '.map', '-Wl,--no-warn-rwx-segments',
            '-o', stem + '.elf'] + (['-u', '_printf_float'] if a.pfloat else []) + a.ldflag +
           ['-Wl,--start-group', '-lc', '-lm', '-lgcc', '-Wl,--end-group'])
    r = subprocess.run(cmd)
    if r.returncode:
        sys.exit(r.returncode)
    r = subprocess.run([sys.executable, '-I', os.path.join(SDK, 'mkimage.py'), stem + '.elf', a.out,
                        '--stack', str(int(a.stack, 0)), '--heap', str(int(a.heap, 0)), '--api', a.api] +
                       (['--psram'] if a.psram else []) + (['--fixed'] if a.fixed else []) +
                       (['--module'] if a.module else []))
    sys.exit(r.returncode)


if __name__ == '__main__':
    main()
