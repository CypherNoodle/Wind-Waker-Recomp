#!/usr/bin/env bash
# Patch a USA Wind Waker disc with Better Wind Waker for BlueWake's Mods menu.
#
#   scripts/mods/make_betterww_iso.sh "/path/to/The Legend Of Zelda The Wind Waker.iso" [OUT.iso]
#
# BlueWake runs Better Wind Waker's code from variants compiled into the app,
# built from one exact patched executable: Better Wind Waker at the commit
# below with its default settings (its settings.txt). This script produces
# that disc and checks it; copy it to the iPad as Documents/BlueWake/Mods/
# betterww.iso (Files app: On My iPad > BlueWake > BlueWake > Mods).
# Needs python3 with PyYAML and Pillow (pip3 install pyyaml pillow).
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
BETTERWW_URL=https://github.com/WideBoner/betterww.git
BETTERWW_SHA=45014813424a7aebf7e4092fd26cae8f632837ae
EXPECTED_DOL_SHA1=e884a349a28ca534e272cf4c17737db587245cdd

iso=${1:?usage: make_betterww_iso.sh CLEAN.iso [OUT.iso]}
out=${2:-$root/build/mods/betterww.iso}
mkdir -p "$(dirname "$out")"
if [ "$(cd "$(dirname "$iso")" && pwd)/$(basename "$iso")" = \
     "$(cd "$(dirname "$out")" && pwd)/$(basename "$out")" ]; then
    echo "make_betterww_iso: output must not overwrite your original disc" >&2
    exit 1
fi
src=$root/build/mods/betterww-src
if [ ! -d "$src/.git" ]; then
    git clone -q "$BETTERWW_URL" "$src"
fi
if [ -n "$(git -C "$src" status --porcelain --untracked-files=no)" ]; then
    echo "make_betterww_iso: patcher has local changes; use a clean pinned checkout" >&2
    exit 1
fi
git -C "$src" checkout -q "$BETTERWW_SHA"
work=$(mktemp -d)
python3 "$root/scripts/mods/betterww_headless.py" "$src" "$iso" "$work"
sha=$(python3 "$root/scripts/mods/iso_dol_sha1.py" "$work/bluewake.iso")
if [ "$sha" != "$EXPECTED_DOL_SHA1" ]; then
    echo "make_betterww_iso: the patched executable is $sha, not $EXPECTED_DOL_SHA1 (different disc or Better Wind Waker version)" >&2
    exit 1
fi
mv "$work/bluewake.iso" "$out"
rmdir "$work"
echo "make_betterww_iso: wrote $out (executable $sha)"
