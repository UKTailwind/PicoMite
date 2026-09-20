#!/bin/sh
#  setup.sh - Prince of Pico, everything in one go, on Linux and macOS.
#
#    ./setup.sh                    fetch the source release and convert it
#    ./setup.sh ~/path/to/copy     use a copy you already have
#
#  Leaves a directory called board/ holding exactly the files that go on the
#  board's drive.  What ends up in there is the game's own content in another
#  format and is not yours to pass on - see README.md.

set -e
cd "$(dirname "$0")"

PY=""
for candidate in python3 python; do
    if command -v "$candidate" >/dev/null 2>&1; then
        # python may still be a 2.x on an old box, so ask it.
        if "$candidate" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' 2>/dev/null; then
            PY="$candidate"
            break
        fi
    fi
done

if [ -z "$PY" ]; then
    echo
    echo "Python 3.8 or later was not found on the path."
    echo
    echo "On Debian or Ubuntu:  sudo apt install python3"
    echo "On Fedora:            sudo dnf install python3"
    echo "On macOS:             brew install python3, or from python.org"
    echo
    exit 1
fi

exec "$PY" setup.py "$@"
