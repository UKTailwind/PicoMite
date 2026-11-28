"""make.py: curl.app for the PicoMite (the PC3 build: an RP2350 with PSRAM and the Pico 2 W's Wi-Fi), curl 8.23.0's
own command-line tool with libcurl, mbedTLS 3.6.8 (TLS 1.2 and 1.3) and zlib 1.3.1, on the firmware's network
services (loader/pmimg.h: net_poll, net_dns, sock_*). The Colour Maximite 2's apps/curl (CMM2V1.5 v8-dev), its port/
and curl_main.c with the PicoMite's names, its services' version (NET_API) and the RP2350's TRNG for TLS's entropy.
The three are fetched the first time, as their release archives (SHA-256 checked), into apps/curl/src (git ignores
it), and built as released.

    python apps/curl/make.py [--new-ca]

makes out/curl.app, and out/zip/ as it goes into the programs zip (as the CMM2's make.py does):
    curl/curl.app, curl/cacert.pem               curl's CA bundle from https://curl.se/ca/ (its .sha256 checked),
                                                 fetched once (--new-ca fetches it again, for a release)
    licences/curl-COPYING.txt, MbedTLS-LICENSE.txt, zlib-LICENSE.txt, MPL-2.0.txt (cacert.pem's)
Copy the curl folder to B: (or A:), then at the prompt (RUN's arguments are a string on the PicoMite):
    RUN "B:/curl/curl.app", "-o B:/page.html https://example.com/"
    RUN "B:/curl/curl.app", "--cmm2-bench 192.168.1.22 8088 /8mb.bin"   (an HTTP download's speed, nothing kept)
The user's guide: curl_guide.md, beside this file.
"""
import glob, hashlib, os, re, shutil, subprocess, sys, tarfile, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, 'src')
OUT = os.path.join(HERE, 'out')
PORT = os.path.join(HERE, 'port')

PACKAGES = {                                    # folder: (url, SHA-256 of the archive)
    'curl-8.23.0': ('https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.xz',
                    '8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5'),
    'mbedtls-3.6.8': ('https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.8/mbedtls-3.6.8.tar.bz2',
                      'bc54b818e87f49c0b3281dd85d24adaede0e63b4e822e3fbd49c005a38b9595b'),
    'zlib-1.3.1': ('https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.xz',
                   '38ef96b8dfe510d42707d9c781877914792541133e1870841463bfa73f883e32'),
}
CURL = os.path.join(SRC, 'curl-8.23.0')
MBEDTLS = os.path.join(SRC, 'mbedtls-3.6.8')
ZLIB = os.path.join(SRC, 'zlib-1.3.1')
CA_URL = 'https://curl.se/ca/cacert.pem'
MPL_URL = 'https://www.mozilla.org/media/MPL/2.0/index.txt'   # cacert.pem's licence (curl.se/docs/caextract.html)

FLAGS = ['-DHAVE_CONFIG_H', '-DBUILDING_LIBCURL', '-DCURL_STATICLIB', '-DNET_API=1',
         '-DMBEDTLS_USER_CONFIG_FILE=<mbedtls_user_config.h>',
         '-Wno-format']                         # (curl prints uint32_t, unsigned long here, with %u: the same 32 bits)
LDFLAGS = ['-Wl,--wrap=fopen', '-Wl,--wrap=fdopen',   # (port/cmm2_stdio.c: 16 KB file buffers,
           '-Wl,--wrap=fgets', '-Wl,--wrap=_write']    # CR LF lines read as LF, Ctrl-C while it prints)
STACK = 64 * 1024                               # from the SRAM heap (the CMM2's is 128 KB, from its own RAM)


def fetch_all():
    """each package's folder, from its release archive the first time"""
    for name, (url, sha) in PACKAGES.items():
        folder = os.path.join(SRC, name)
        if os.path.isdir(folder):
            continue
        os.makedirs(SRC, exist_ok=True)
        arc = os.path.join(SRC, os.path.basename(url))
        if not os.path.exists(arc):
            print('fetching', url, flush=True)
            urllib.request.urlretrieve(url, arc)
        got = hashlib.sha256(open(arc, 'rb').read()).hexdigest()
        if got != sha:
            sys.exit('%s: SHA-256 %s, not %s' % (arc, got, sha))
        with tarfile.open(arc) as t:
            t.extractall(SRC, filter='data')


def makefile_list(path, var):
    """the files a Makefile.inc variable lists (VAR = a.c b.c \\ ...), with $(OTHER) expanded"""
    text = open(path).read().replace('\\\n', ' ')
    m = re.search(r'^%s\s*=(.*)$' % var, text, re.M)
    if not m:
        sys.exit('%s: no %s' % (path, var))
    out = []
    for w in m.group(1).split():
        sub = re.fullmatch(r'\$\((\w+)\)', w)
        out += makefile_list(path, sub.group(1)) if sub else [w]
    return out


def sources():
    """libcurl, curl's command-line tool and its manual, mbedTLS, zlib and port/"""
    lib = os.path.join(CURL, 'lib')
    s = []
    for var in ('LIB_CFILES', 'LIB_VAUTH_CFILES', 'LIB_VDNS_CFILES', 'LIB_VTLS_CFILES', 'LIB_VQUIC_CFILES',
                'LIB_CURLX_CFILES'):
        s += [os.path.join(lib, f) for f in makefile_list(os.path.join(lib, 'Makefile.inc'), var)]
    src = os.path.join(CURL, 'src')
    s += [os.path.join(src, f) for f in makefile_list(os.path.join(src, 'Makefile.inc'), 'CURL_CFILES')]
    s += [os.path.join(src, 'tool_hugehelp.c')]
    s += [os.path.join(ZLIB, f) for f in ('adler32.c', 'crc32.c', 'inflate.c', 'inftrees.c', 'inffast.c', 'zutil.c')]
    s += sorted(glob.glob(os.path.join(MBEDTLS, 'library', '*.c')))
    s += [os.path.join(PORT, f) for f in ('cmm2_net.c', 'cmm2_tls.c', 'cmm2_stdio.c')]
    return s + [os.path.join(HERE, 'curl_main.c')]


def includes():
    return [os.path.join(PORT, 'include'), PORT, os.path.join(CURL, 'include'), os.path.join(CURL, 'lib'),
            os.path.join(CURL, 'src'), ZLIB, os.path.join(MBEDTLS, 'include')]


def ca_bundle(dest, new=False):
    """curl's CA bundle at dest, checked against the SHA-256 curl.se publishes beside it"""
    if os.path.exists(dest) and not new:
        return
    pem = urllib.request.urlopen(CA_URL).read()
    want = urllib.request.urlopen(CA_URL + '.sha256').read().decode().split()[0]
    if hashlib.sha256(pem).hexdigest() != want:
        sys.exit('%s: its SHA-256 is not the one curl.se gives' % CA_URL)
    open(dest, 'wb').write(pem)
    when = re.search(rb'as of: (.*)', pem)
    print('cacert.pem: %d bytes, %d certificates, Mozilla\'s of %s' % (len(pem), pem.count(b'BEGIN CERTIFICATE'),
          when.group(1).decode().strip() if when else '?'))


def main():
    fetch_all()
    os.makedirs(OUT, exist_ok=True)
    listfile = os.path.join(OUT, 'curl.sources')
    open(listfile, 'w').write('\n'.join(sources()) + '\n')
    app = os.path.join(OUT, 'curl.app')
    cmd = [sys.executable, '-I', os.path.join(REPO, 'tools', 'sdk', 'build.py'), app,
           '--sources', listfile, '--api', '1', '--psram', '--stack', str(STACK)] + \
          ['--cflag=' + f for f in FLAGS] + sum([['-I', i] for i in includes()], []) + \
          ['--ldflag=' + f for f in LDFLAGS]
    subprocess.run(cmd, check=True)
    new = '--new-ca' in sys.argv
    ca_bundle(os.path.join(OUT, 'cacert.pem'), new)
    zipdir = os.path.join(OUT, 'zip')               # the programs zip's curl folder and the licences
    os.makedirs(os.path.join(zipdir, 'curl'), exist_ok=True)
    os.makedirs(os.path.join(zipdir, 'licences'), exist_ok=True)
    shutil.copyfile(app, os.path.join(zipdir, 'curl', 'curl.app'))
    shutil.copyfile(os.path.join(OUT, 'cacert.pem'), os.path.join(zipdir, 'curl', 'cacert.pem'))
    for src, name in ((os.path.join(CURL, 'COPYING'), 'curl-COPYING.txt'),
                      (os.path.join(MBEDTLS, 'LICENSE'), 'MbedTLS-LICENSE.txt'),
                      (os.path.join(ZLIB, 'LICENSE'), 'zlib-LICENSE.txt')):
        shutil.copyfile(src, os.path.join(zipdir, 'licences', name))
    mpl = os.path.join(zipdir, 'licences', 'MPL-2.0.txt')
    if not os.path.exists(mpl):
        text = urllib.request.urlopen(MPL_URL).read()
        if not text.startswith(b'Mozilla Public License Version 2.0'):
            sys.exit(MPL_URL + ': not the MPL 2.0')
        open(mpl, 'wb').write(text)
    print('%s: %d bytes; out/zip ready for the programs zip' % (app, os.path.getsize(app)))


if __name__ == '__main__':
    main()
