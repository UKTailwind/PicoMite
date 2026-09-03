#!/usr/bin/env bash
#
# setup-tinyusb.sh - create the patched TinyUSB 0.21 tree PicoMite builds against.
#
# PicoMite's USB-host driver fixes (fast USB flash-drive transfers and reliable
# enumeration of several devices behind a hub) are carried as patches on top of
# TinyUSB 0.21.0, in a SIBLING directory ../tinyusb-0.21 (next to this repo, not
# inside it) that PICO_TINYUSB_PATH in CMakeLists.txt points at. The Pico SDK's
# own bundled TinyUSB is left untouched.
#
# Run this once, before the first build. To recreate the tree, delete
# ../tinyusb-0.21 and run it again. Works on Linux, macOS and Git Bash.
#
set -euo pipefail

TAG=0.21.0
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # tinyusb-patches/
REPO="$(cd "$HERE/.." && pwd)"                          # PicoMite/
DEST="$(cd "$REPO/.." && pwd)/tinyusb-0.21"             # ../tinyusb-0.21

command -v git   >/dev/null 2>&1 || { echo "error: 'git' not found in PATH"; exit 1; }
command -v patch >/dev/null 2>&1 || { echo "error: 'patch' not found in PATH (Git for Windows provides it)"; exit 1; }

if [ -e "$DEST" ]; then
  echo "error: $DEST already exists."
  echo "       to recreate it, remove it first:  rm -rf \"$DEST\""
  exit 1
fi

echo "Cloning TinyUSB $TAG -> $DEST"
# core.autocrlf=false so the checkout is LF, matching the LF patches applied below.
# --depth 1 keeps it shallow; TinyUSB's lib/ submodules are not needed for the
# PicoMite build (they are only used by FreeRTOS/ThreadX/RTT configurations).
git -c advice.detachedHead=false -c core.autocrlf=false \
    clone --depth 1 --branch "$TAG" https://github.com/hathach/tinyusb "$DEST"

echo "Applying PicoMite patches (patch -p1):"
apply() {  # $1 = patch basename, $2 = target file relative to the tree root
  echo "  - $1.patch"
  # Normalize the target to LF first: TinyUSB's .gitattributes can force a CRLF
  # checkout, and the patches are LF, so this makes the apply deterministic.
  sed -i 's/\r$//' "$DEST/$2"
  patch -p1 -d "$DEST" < "$HERE/$1.patch"
}
apply hcd_rp2040 src/portable/raspberrypi/rp2040/hcd_rp2040.c
apply rp2040_usb src/portable/raspberrypi/rp2040/rp2040_usb.c
apply usbh       src/host/usbh.c

echo
echo "Done. ../tinyusb-0.21 is ready; PicoMite builds against it via PICO_TINYUSB_PATH."
