#!/usr/bin/env bash
# Run the macOS host on the game module your device build translated, with or
# without in-between frames (Display > Smooth Motion on iOS), for testing.
#
# Needs a device build (build/device) and a Mac host and module:
#   host:   cmake -S scripts/builder/training -B build/mac-interp/host -G Ninja
#           -DCMAKE_BUILD_TYPE=Release -DAURORA_DAWN_PROVIDER=package ... (as the Builder's training)
#   module: python3 scripts/ios/retag_macho_platform.py --platform macos --minos 14.0
#           build/device/composite-ios/gGZLE01_recomp.dylib build/mac-interp/composite/gGZLE01_recomp.dylib
#           && codesign -f -s - build/mac-interp/composite/gGZLE01_recomp.dylib
#
#   run_host.sh OUT_DIR INTERP(0|1) RETRACES [title|outset|save|load] [DUMP_FROM DUMP_TO] [HOST]
#
# title:  boot to the title flyover (no input).
# outset: a new game through the opening to control on Outset (about 6.5 minutes).
# save:   the same, then the pause-menu save and a quit to the title; use with
#         RENDERER=headless PACE=0 (about 2 minutes). Copy OUT_DIR/test.card to
#         build/mac-interp/saves/outset-start.card to keep it.
# load:   boots CARD (default build/mac-interp/saves/outset-start.card) and loads
#         slot 1: gameplay on Outset at retrace ~705 instead of ~20,400.
#         WALK=1 holds the stick from retrace 900 (WALK_X/WALK_Y, default 0/-127;
#         0/127 runs Link back down the pier with the camera turning).
# RENDERER=headless and PACE=0 override the window and real-time pacing.
# ASPECT=16:10|16:9 widescreen (4:3 default), WINDOW=WxH, FULLSCREEN=1, SCALE=N.
# BWW=1 Better Wind Waker's options (their defaults); OPTIONS=name,-name,... changes them.
# DUMP_FROM/DUMP_TO: game frames (presents, not retraces) whose real and
# in-between images are written to OUT_DIR/dump (see frame_interp_report.py).
set -u
if [ $# -lt 3 ]; then sed -n '2,34p' "$0"; exit 2; fi
# The main checkout (build/ lives there), also when run from a git worktree.
ROOT=$(dirname "$(git -C "$(dirname "$0")" rev-parse --path-format=absolute --git-common-dir)")
out=$1; interp=$2; retraces=$3; route=${4:-title}; dump_from=${5:-}; dump_to=${6:-}
host=${7:-$ROOT/build/mac-interp/host/host/bluewake_host}
# The newest module with the mods (scripts/mods/build_mods.sh): Better Wind
# Waker's options and 16:10, else 16:10, else the plain one.
default_composite=$ROOT/build/mac-interp/composite/gGZLE01_recomp.dylib
for m in composite-1610 composite-options; do
  [ -f "$ROOT/build/mac-interp/$m/gGZLE01_recomp.dylib" ] && \
      default_composite=$ROOT/build/mac-interp/$m/gGZLE01_recomp.dylib
done
composite=${COMPOSITE:-$default_composite}
mkdir -p "$out"
[ "$route" = load ] || rm -f "$out/test.card"
env_args=(
  BLUEWAKE_ROOT="$ROOT" BLUEWAKE_RENDERER="${RENDERER:-aurora}"
  BLUEWAKE_DOL="$ROOT/build/device/game/main.dol" BLUEWAKE_DISC="$ROOT/build/personal/GZLE01.iso"
  BLUEWAKE_RELS_DIR="$ROOT/build/device/game/rels" BLUEWAKE_CARD_PATH="$out/test.card" BLUEWAKE_CARD_LOG=1
  BLUEWAKE_DSP_IROM="$ROOT/ref/recompcore/Data/Sys/GC/dsp_rom.bin"
  BLUEWAKE_DSP_COEF="$ROOT/ref/recompcore/Data/Sys/GC/dsp_coef.bin"
  BLUEWAKE_MAX_BLOCKS=100000000000 BLUEWAKE_MAX_RETRACES="$retraces"
  BLUEWAKE_CYCLE_CAP=16384 BLUEWAKE_DSP_MODE=hle BLUEWAKE_WALL_PACE="${PACE:-1}"
  BLUEWAKE_PERF_LOG=1 DOL_FRAME_PACING_LOG=1 DOL_AURORA_SHOW_FPS=1
  DOL_AURORA_FRAME_INTERP="$interp" DOL_AURORA_FRAME_INTERP_LOG=1
  # A personal save made by `save` lives in build/, never in Git.
)
if [ "$route" = outset ] || [ "$route" = save ]; then
  script=""
  last=22000; [ "$route" = save ] && last=29800
  for n in $(seq 17800 150 $last); do script="${script:+$script,}$n:0x0100:2"; done
  env_args+=(BLUEWAKE_PAD_BUTTONS=0x0100 BLUEWAKE_PAD_PULSE_ON_TITLE_READY=1
             BLUEWAKE_PAD_PULSE_LENGTH=2 BLUEWAKE_PAD_SCRIPT="$script" BLUEWAKE_PLAYER_PROBE=1)
  [ "$route" = outset ] && env_args+=(BLUEWAKE_PAD_CONFIRM_EVENT=any)
  [ "$route" = save ] && env_args+=(BLUEWAKE_SAVE_ROUTE=1)
fi
if [ "$route" = load ]; then
  cp "${CARD:-$ROOT/build/mac-interp/saves/outset-start.card}" "$out/test.card"
  script=""
  # A through the title and file select (the play scene is up by retrace 705),
  # then WALK=1 holds the stick forward from retrace 900 for WALK_LEN retraces.
  last_a=1600; [ "${WALK:-0}" = 1 ] && last_a=760
  for n in $(seq 340 60 $last_a); do script="${script:+$script,}$n:0x0100:2"; done
  [ "${WALK:-0}" = 1 ] && script="$script,900:0x0000:${WALK_LEN:-900}:${WALK_X:-0}:${WALK_Y:--127}"
  # EXTRA_PAD: more presses for the pad script, retrace:buttons:length[:x:y],...
  # (START is 0x1000, A 0x0100, B 0x0200, Z 0x0010).
  [ -n "${EXTRA_PAD:-}" ] && script="$script,$EXTRA_PAD"
  env_args+=(BLUEWAKE_PAD_BUTTONS=0x0100 BLUEWAKE_PAD_PULSE_ON_TITLE_READY=1
             BLUEWAKE_PAD_PULSE_LENGTH=2 BLUEWAKE_PAD_CONFIRM_EVENT=any
             BLUEWAKE_PAD_SCRIPT="$script" BLUEWAKE_PLAYER_PROBE=1)
fi
# The picture: ASPECT=4:3|16:10|16:9 (the widescreen mods), WINDOW=WxH points,
# FULLSCREEN=1, SCALE=0 (window pixels) or 1-4 (x 480 lines).
[ -n "${ASPECT:-}" ] && env_args+=(BLUEWAKE_ASPECT="$ASPECT")
# Better Wind Waker's options (mods/betterww/options.txt): BWW=1 turns on the
# defaults, OPTIONS=name,-name,... (or none,name,...) adjusts them.
[ "${BWW:-0}" = 1 ] && env_args+=(BLUEWAKE_MODS=betterww)
[ -n "${OPTIONS:-}" ] && env_args+=(BLUEWAKE_OPTIONS="$OPTIONS")
[ -n "${WINDOW:-}" ] && env_args+=(DOL_AURORA_WINDOW="$WINDOW")
[ -n "${FULLSCREEN:-}" ] && env_args+=(DOL_AURORA_FULLSCREEN="$FULLSCREEN")
[ -n "${SCALE:-}" ] && env_args+=(DOL_AURORA_RENDER_SCALE="$SCALE")
if [ -n "$dump_from" ]; then
  mkdir -p "$out/dump"
  env_args+=(DOL_AURORA_FRAME_INTERP_DUMP="$out/dump" DOL_AURORA_FRAME_INTERP_DUMP_FROM="$dump_from"
             DOL_AURORA_FRAME_INTERP_DUMP_TO="$dump_to")
fi
cd "$out"
exec env -i HOME="$HOME" PATH=/usr/bin:/bin ${EXTRA_ENV:-} "${env_args[@]}" /usr/bin/time -l "$host" "$composite" > "$out/run.log" 2>&1
