#!/usr/bin/env python3
"""
fetch_ca.py - Download well-known CA root certificates as a PEM file
              for PicoMite's WEB TLS CA command.

Usage:
    python fetch_ca.py                              # default: ISRG Root X1
    python fetch_ca.py --list                       # show available roots
    python fetch_ca.py isrg-x1 digicert-g2          # bundle multiple
    python fetch_ca.py -o my-ca.pem isrg-x1         # custom filename
    python fetch_ca.py --verify httpbin.org:443 isrg-x1
        # download AND verify by connecting to a test server with the new
        # bundle and confirming its chain validates

Memory: keep the bundle small. The PicoMite parses every cert into mbedtls
state on each handshake; ~5-10 KB total is the sweet spot. Each root is
~1-2 KB so 3-5 roots is comfortable. The full Mozilla bundle (~150 roots,
~200 KB) will NOT fit in the 40 KB lwIP heap and will fail to parse.

The PicoMite-side flow once you have a PEM:
    WEB NTP                                ' real time required for expiry check
    WEB TLS CA "ca.pem"                    ' enables MBEDTLS_SSL_VERIFY_REQUIRED
    WEB OPEN TLS CLIENT "httpbin.org", 443
"""

import argparse
import base64
import socket
import ssl
import sys
import urllib.request
from pathlib import Path

# Curated list of common public CA roots. Each entry is (description, URL).
# All URLs point at PEM-format files except where noted; the script auto-
# converts DER (binary, starts with 0x30) to PEM on the fly.
ROOTS = {
    "isrg-x1": (
        "ISRG Root X1 - Let's Encrypt (most public HTTPS incl. httpbin.org)",
        "https://letsencrypt.org/certs/isrgrootx1.pem",
    ),
    "isrg-x2": (
        "ISRG Root X2 - Let's Encrypt ECDSA (smaller, ECDSA-signed sites)",
        "https://letsencrypt.org/certs/isrg-root-x2.pem",
    ),
    "digicert-g2": (
        "DigiCert Global Root G2 - GitHub, AWS, many enterprise sites",
        "https://cacerts.digicert.com/DigiCertGlobalRootG2.crt.pem",
    ),
    "digicert-g3": (
        "DigiCert Global Root G3 - ECDSA variant of G2",
        "https://cacerts.digicert.com/DigiCertGlobalRootG3.crt.pem",
    ),
    "globalsign-r6": (
        "GlobalSign Root R6 - widely used for enterprise / IoT cloud",
        "https://secure.globalsign.com/cacert/root-r6.crt",
    ),
    "amazon-r1": (
        "Amazon Root CA 1 - AWS-hosted sites (httpbin.org, S3, CloudFront)",
        "https://www.amazontrust.com/repository/AmazonRootCA1.pem",
    ),
    "amazon-r2": (
        "Amazon Root CA 2 - 4096-bit RSA AWS root",
        "https://www.amazontrust.com/repository/AmazonRootCA2.pem",
    ),
    "amazon-r3": (
        "Amazon Root CA 3 - ECDSA AWS root",
        "https://www.amazontrust.com/repository/AmazonRootCA3.pem",
    ),
    "amazon-r4": (
        "Amazon Root CA 4 - ECDSA P-384 AWS root",
        "https://www.amazontrust.com/repository/AmazonRootCA4.pem",
    ),
    "starfield-g2": (
        "Starfield Services Root G2 - cross-signs Amazon CAs (transitional)",
        "https://www.amazontrust.com/repository/SFSRootCAG2.pem",
    ),
}

# Sensible default for a smoke test that needs to cover both LE-signed
# (most public HTTPS) AND AWS-signed (httpbin.org, S3, many APIs) sites.
DEFAULT_ROOTS = ["isrg-x1", "amazon-r1"]


def fetch(url: str, timeout: int = 30) -> bytes:
    """Download URL into bytes. Uses the system trust store for the TLS
    connection to the CA provider itself."""
    req = urllib.request.Request(url, headers={"User-Agent": "fetch_ca/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def to_pem(data: bytes) -> str:
    """Return data as a PEM-encoded certificate string.
    Accepts PEM (passes through after CRLF normalisation) or DER (binary,
    converts to PEM)."""
    # PEM is ASCII text containing "-----BEGIN CERTIFICATE-----"
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError:
        text = ""
    if "-----BEGIN CERTIFICATE-----" in text:
        return text.replace("\r\n", "\n").strip() + "\n"
    # Otherwise treat as DER and wrap in PEM armor
    b64 = base64.b64encode(data).decode("ascii")
    body = "\n".join(b64[i : i + 64] for i in range(0, len(b64), 64))
    return f"-----BEGIN CERTIFICATE-----\n{body}\n-----END CERTIFICATE-----\n"


def verify(host_port: str, pem_path: Path) -> None:
    """Connect to host:port and validate its certificate chain against the
    PEM bundle. Raises ssl.SSLCertVerificationError on failure."""
    if ":" not in host_port:
        raise ValueError(f"Expected HOST:PORT, got '{host_port}'")
    host, port_str = host_port.rsplit(":", 1)
    port = int(port_str)

    ctx = ssl.create_default_context(cafile=str(pem_path))
    # Match what PicoMite mbedtls will do: REQUIRED verification, hostname check.
    ctx.check_hostname = True
    ctx.verify_mode = ssl.CERT_REQUIRED

    with socket.create_connection((host, port), timeout=10) as raw:
        with ctx.wrap_socket(raw, server_hostname=host) as s:
            cert = s.getpeercert()
            subj = dict(x[0] for x in cert.get("subject", []))
            issuer = dict(x[0] for x in cert.get("issuer", []))
            print(f"  Connected: {host}:{port}")
            print(f"  Subject CN: {subj.get('commonName', '?')}")
            print(f"  Issuer CN:  {issuer.get('commonName', '?')}")
            print(f"  Valid until: {cert.get('notAfter', '?')}")


def main() -> int:
    p = argparse.ArgumentParser(
        description=__doc__.strip().split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "roots",
        nargs="*",
        default=DEFAULT_ROOTS,
        help="Root names to bundle (default: isrg-x1). See --list.",
    )
    p.add_argument(
        "-o",
        "--output",
        default="ca.pem",
        help="Output filename (default: ca.pem)",
    )
    p.add_argument(
        "--list", action="store_true", help="List available well-known roots"
    )
    p.add_argument(
        "--verify",
        metavar="HOST:PORT",
        help="After download, verify the bundle by connecting to HOST:PORT",
    )
    args = p.parse_args()

    if args.list:
        print("Available roots:\n")
        for key, (desc, _) in ROOTS.items():
            print(f"  {key:14}  {desc}")
        print("\nDefault if none specified: " + ", ".join(DEFAULT_ROOTS))
        return 0

    unknown = [r for r in args.roots if r not in ROOTS]
    if unknown:
        print(f"Unknown root(s): {', '.join(unknown)}", file=sys.stderr)
        print("Use --list to see available names.", file=sys.stderr)
        return 1

    parts = []
    for key in args.roots:
        desc, url = ROOTS[key]
        print(f"Fetching {key} from {url} ...")
        try:
            data = fetch(url)
        except Exception as e:
            print(f"  ERROR: {e}", file=sys.stderr)
            return 1
        pem = to_pem(data)
        if "-----BEGIN CERTIFICATE-----" not in pem:
            print(f"  ERROR: downloaded data is not a certificate", file=sys.stderr)
            return 1
        # Header comment is allowed by mbedtls (parser only cares about
        # text between BEGIN/END markers). Useful for humans inspecting
        # the file later.
        parts.append(f"# {key}: {desc}\n# Source: {url}\n{pem}\n")

    out = Path(args.output)
    # newline="\n" forces LF line endings so the file is byte-identical
    # across Windows and Unix, and matches what mbedtls expects.
    out.write_text("".join(parts), encoding="ascii", newline="\n")
    size = out.stat().st_size
    print(f"\nWrote {len(args.roots)} certificate(s) to {out} ({size} bytes)")

    if args.verify:
        print(f"\nVerifying bundle against {args.verify} ...")
        try:
            verify(args.verify, out)
            print("  OK - chain validates against the new bundle")
        except ssl.SSLCertVerificationError as e:
            print(f"  VERIFY FAILED: {e}", file=sys.stderr)
            print(
                "  The server's cert chains to a root that isn't in your bundle.\n"
                "  Use --list and add the right one, or pick a different test server.",
                file=sys.stderr,
            )
            return 2
        except Exception as e:
            print(f"  ERROR: {e}", file=sys.stderr)
            return 2

    print(f"\nNext steps on the PicoMite:")
    print(f"  - Transfer {out.name} to the device's filesystem")
    print(f"  - In BASIC:")
    print(f"      WEB NTP")
    print(f'      WEB TLS CA "{out.name}"')
    print(f'      WEB OPEN TLS CLIENT "host", 443')
    return 0


if __name__ == "__main__":
    sys.exit(main())
