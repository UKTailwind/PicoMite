#!/usr/bin/env bash
#
# setup-tinyusb.sh - create the patched TinyUSB tree PicoMite builds against.
#
# PicoMite's USB-host driver fixes (fast USB flash-drive transfers and reliable
# enumeration of several devices behind a hub) are carried as patches on top of
# TinyUSB master at commit e42fa9357 (2026-10-07), in a SIBLING directory
# ../tinyusb-master (next to this repo, not inside it) that PICO_TINYUSB_PATH in
# CMakeLists.txt points at. The Pico SDK's own bundled TinyUSB is left untouched.
#
# Run this once, before the first build. To recreate the tree, delete
# ../tinyusb-master and run it again. Works on Linux, macOS and Git Bash.
#
set -euo pipefail

COMMIT=e42fa93570a672d4779b7a3d0e14145a3ae919e6         # TinyUSB master, 2026-10-07
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # tinyusb-patches/
REPO="$(cd "$HERE/.." && pwd)"                          # PicoMite/
DEST="$(cd "$REPO/.." && pwd)/tinyusb-master"           # ../tinyusb-master

command -v git   >/dev/null 2>&1 || { echo "error: 'git' not found in PATH"; exit 1; }
command -v patch >/dev/null 2>&1 || { echo "error: 'patch' not found in PATH (Git for Windows provides it)"; exit 1; }

if [ -e "$DEST" ]; then
  echo "error: $DEST already exists."
  echo "       to recreate it, remove it first:  rm -rf \"$DEST\""
  exit 1
fi

echo "Fetching TinyUSB master ${COMMIT:0:9} -> $DEST"
# The commit is not a release, so fetch that one commit by its hash.
# core.autocrlf=false so the checkout is LF, matching the LF patches applied
# below. --depth 1 keeps it shallow; TinyUSB's lib/ submodules are not needed
# for the PicoMite build (they are only used by FreeRTOS/ThreadX/RTT
# configurations).
git -c init.defaultBranch=master init -q "$DEST"
git -C "$DEST" config core.autocrlf false
git -C "$DEST" remote add origin https://github.com/hathach/tinyusb
git -C "$DEST" fetch -q --depth 1 origin "$COMMIT"
git -C "$DEST" -c advice.detachedHead=false checkout -q FETCH_HEAD

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

echo
echo "Done. ../tinyusb-master is ready; PicoMite builds against it via PICO_TINYUSB_PATH."
