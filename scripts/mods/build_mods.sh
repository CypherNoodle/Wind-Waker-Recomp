#!/usr/bin/env bash
# Add BlueWake's code mods (widescreen, Better Wind Waker, and both together) to a
# device build's composite source, after scripts/ios/build_device.sh has built it.
#
#   scripts/mods/build_mods.sh BUILD_DIR DISC.iso
#
# BUILD_DIR is build_device.sh's --out (default build/device). Afterwards, run
# the composite build again (cmake BUILD_DIR/composite-ios && ninja -C
# BUILD_DIR/composite-ios); only the variant chunks and module_export.c compile.
# See docs/MODS.md.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
B=$(cd "${1:?usage: build_mods.sh BUILD_DIR DISC.iso}" && pwd)
iso=${2:?usage: build_mods.sh BUILD_DIR DISC.iso}
M=$B/mods
mkdir -p "$M"
dolrecomp=$B/dolrecomp/dolrecomp
BWW_SHA1=e884a349a28ca534e272cf4c17737db587245cdd

translate() {  # DOL OUT_DIR [RELS_DIR]
    "$dolrecomp" --gamecube --backend c --cpu gekko --partition-instructions 4096 "$1" "$2/dol" -j 8 >"$2.log" 2>&1
    if [ $# -gt 2 ]; then
        "$dolrecomp" --gamecube --backend c --cpu gekko --rel-base 0xC0400000 "$3" "$2/rels" -j 8 >>"$2.log" 2>&1
    fi
}
composite() {  # DOL_DIR RELS_DIR RELS_BIN MAIN_DOL OUT
    python3 "$root/scripts/generate_composite.py" --dol-dir "$1" --rels-dir "$2" --rels-bin-dir "$3" \
        --main-dol "$4" --output-dir "$5" | tail -1
}

echo "==> widescreen"
mkdir -p "$M/widescreen"
python3 "$root/scripts/mods/gecko_apply.py" "$root/mods/widescreen/GZLE01.gecko" "$B/game/main.dol" \
    "$M/widescreen/main.dol" "$M/widescreen/runtime.json"
translate "$M/widescreen/main.dol" "$M/widescreen/translated"
composite "$M/widescreen/translated/dol/generated" "$B/translated/rels/generated/rels" "$B/game/rels" \
    "$M/widescreen/main.dol" "$M/widescreen/composite-src"

echo "==> Better Wind Waker"
"$root/scripts/mods/make_betterww_iso.sh" "$iso" "$M/betterww.iso"
mkdir -p "$M/betterww"
# The app's own importer, current source (it accepts the patched executable).
clang -O2 -o "$M/disc_extract" "$root/scripts/ios/disc_extract.c" "$root/apple/ios/src/disc_import.c"     -I"$root/apple/ios/src"
BLUEWAKE_ACCEPT_DOL_SHA1=$BWW_SHA1 "$M/disc_extract" "$M/betterww.iso" "$M/betterww/game" 2>/dev/null
echo
translate "$M/betterww/game/main.dol" "$M/betterww/translated" "$M/betterww/game/rels"
composite "$M/betterww/translated/dol/generated" "$M/betterww/translated/rels/generated/rels" \
    "$M/betterww/game/rels" "$M/betterww/game/main.dol" "$M/betterww/composite-src"

echo "==> widescreen + Better Wind Waker"
mkdir -p "$M/combo"
python3 "$root/scripts/mods/gecko_apply.py" "$root/mods/widescreen/GZLE01.gecko" "$M/betterww/game/main.dol" \
    "$M/combo/main.dol" "$M/combo/runtime.json"
translate "$M/combo/main.dol" "$M/combo/translated"
composite "$M/combo/translated/dol/generated" "$M/betterww/translated/rels/generated/rels" \
    "$M/betterww/game/rels" "$M/combo/main.dol" "$M/combo/composite-src"

echo "==> variants into $B/composite-src"
python3 "$root/scripts/generate_composite.py" --dol-dir "$B/translated/dol/generated" \
    --rels-dir "$B/translated/rels/generated/rels" --rels-bin-dir "$B/game/rels" --main-dol "$B/game/main.dol" \
    --output-dir "$M/composite-src.base" | tail -1
python3 "$root/scripts/mods/build_mod_variants.py" --composite-src "$M/composite-src.base" --base-dol "$B/game/main.dol" \
    --mod "widescreen:$M/widescreen/composite-src:$M/widescreen/main.dol:$M/widescreen/runtime.json" \
    --mod "betterww:$M/betterww/composite-src:$M/betterww/game/main.dol" \
    --combo "widescreen+betterww:$M/combo/composite-src"
if ! cmp -s "$M/composite-src.base/generated.h" "$B/composite-src/generated.h"; then
    echo "build_mods: $B/composite-src was generated from other inputs; rebuild it first" >&2
    exit 1
fi
python3 - "$M/composite-src.base" "$B/composite-src" <<'PYEOF'
import pathlib, shutil, sys
src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
for d in dst.glob("chunks_mod_*"):
    shutil.rmtree(d)
for d in src.glob("chunks_mod_*"):
    shutil.copytree(d, dst / d.name)
for f in ("generated_composite.h", "mod_variants.inc"):
    shutil.copy2(src / f, dst / f)
PYEOF
echo "build_mods: done; rebuild the composite: cmake $B/composite-ios && ninja -C $B/composite-ios"
echo "build_mods: copy $M/betterww.iso to the iPad as Documents/BlueWake/Mods/betterww.iso"
