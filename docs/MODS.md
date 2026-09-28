# Mods

BlueWake has a **Mods** section in the in-game ⋯ menu with three mods. Each takes effect the next time
BlueWake starts, and none of them changes your saves.

| Mod | What it does | What you need |
| --- | --- | --- |
| Widescreen 16:9 | The community 16:9 code (Dolphin's GZLE01 Gecko code): a wider field of view with the HUD placed for 16:9, letterboxed on a 4:3 iPad | Nothing; it is built in |
| HD Texture Pack | Replaces the game's textures with a Dolphin-format pack, such as [Hypatia's HD pack](https://forums.dolphin-emu.org/Thread-hypatia-s-tloz-the-wind-waker-hd-pack-v2-0001a) | The pack's `tex1_…` images in `Documents/BlueWake/Load/Textures/GZLE01` |
| Better Wind Waker | [Better Wind Waker](https://github.com/WideBoner/betterww)'s quality-of-life changes from Wind Waker HD: Swift Sail, instant text, faster block pushing, grappling, climbing and crawling, Tingle chests without the Tuner, no song replays, turning while swinging, a faster Ballad of Gales | A disc patched by `scripts/mods/make_betterww_iso.sh`, copied to `Documents/BlueWake/Mods/betterww.iso` |

On the iPad, `Documents/BlueWake` is **On My iPad › BlueWake › BlueWake** in the Files app.

## Installing

**Widescreen:** turn on *Widescreen 16:9* and restart BlueWake. At the default 3× render resolution it
renders 2560×1440 (the 4:3 picture's height, widened), so the game's letterbox bars line up with the scene;
if an older device struggles, 2× renders 1707×960.

**HD textures:** download a Dolphin-format pack for GZLE01. On the iPad, Hypatia's *Android-Lite* build
(3× resolution, about 530 MB of PNG files) fits comfortably; the full-size PC builds need several GB of
memory. Copy the folder of `tex1_…` images (in Hypatia's pack, the `GZL` folder) into
`Load/Textures/GZLE01`, turn on *HD Texture Pack* and restart. Subfolders are searched, `_mipN`
sidecar mipmaps are used, and PNG and DDS files both work. The menu shows how many textures it found.

**Better Wind Waker:** the Builder makes the patched disc it needs: it fetches Better Wind Waker at the
pinned commit, patches your disc with its default settings and checks the result, writing
`build/device/mods/betterww.iso`. (`scripts/mods/make_betterww_iso.sh DISC.iso` does the same on its own, into
`build/mods/betterww.iso`.) Copy that file to the device with **⋯ › Mods › Install Better Wind Waker…**, turn
on *Better Wind Waker* and restart. A disc patched with other settings or another Better Wind Waker
version is refused (the session log says why): see below for why it must match exactly.

## How code mods work in a static recompilation

BlueWake runs the game from native code translated ahead of time, so a mod that rewrites game code in
memory (a Gecko code, or a patcher's assembly changes) has no effect at runtime: the instructions it
writes are never executed. Code mods are therefore built into the app:

1. The mod is applied to the game's executable and modules on the Mac:
   `scripts/mods/gecko_apply.py` for a Gecko code, Better Wind Waker's own patcher for Better Wind
   Waker (`scripts/mods/make_betterww_iso.sh`).
2. The patched `main.dol` and RELs are translated by the same DolRecomp as the base game and merged by
   `scripts/generate_composite.py` into a composite source tree of their own.
3. `scripts/mods/build_mod_variants.py` compares that tree with the base tree. Every translated chunk
   that differs is added to the base composite under a new name (`chunks_mod_<name>/`), code ranges
   only the mod has (Better Wind Waker's added code section) become extra chunks, and the bytes the mod
   changes in the executable's data and in the relocated REL data become writes. A Gecko code's data
   writes are re-applied at every retrace, as Dolphin does. When two mods change the same chunk, a
   translation of the game patched by both supplies the variant used when both are enabled.
4. At boot the host enables the mods named in `BLUEWAKE_MODS` (the menu sets it):
   `bluewake_composite_apply_mods` points the chunk table at the variants before the first dispatch,
   and the writes go into guest RAM. Chunks never call each other directly, so a variant replaces its
   chunk cleanly, and the base game is untouched when a mod is off.

The chunks include `generated.h`, which is unchanged, and only `module_export.c` sees the dispatcher
with the writable chunk table, so adding mods compiles the new variant chunks only (53 today), not the
748 base chunks.

Because the variants are compiled from one patched executable, Better Wind Waker must be the same
build on the iPad: the app checks the executable's SHA-1 in `Mods/betterww.iso`
(`e884a349…`, Better Wind Waker 4501481 with default settings) and ignores any other disc. The disc
itself supplies Better Wind Waker's changed archives and messages (Link's and the ship's models, the
item icons, the text).

HD textures need no build step. Aurora's Dolphin-compatible texture replacement (the
`tex1_WxH[_m]_<XXH64>[_<palette XXH64>]_<format>` names, with palettes hashed over the entries the
texture uses) is connected to GXRuntime's texture decode: the first time a texture's bytes are seen,
the pack is consulted before decoding, and the replacement is cached under the same key as a decoded
texture would be. Replacements are decoded on a background thread the first time they are used (the original texture
is drawn meanwhile), so a new area shows its HD textures a moment later instead of stalling a frame.

## Measured on the iPad Pro (M2)

These results use the developer's optimized build. They do not establish performance parity for a
fresh player build; see [the current performance comparison](BUILDER.md#optimization-profiles).

- Widescreen and the HD pack together, on the pier after loading slot 1: 30 FPS at 100% speed, no late
  frames over more than a minute, main thread about 82% busy (the same as without mods). 5,739 of
  the pack's textures registered; the first file-select frame with new textures took 83 ms.
- All three on the iPad: loading slot 1 and walking the village at 29.9-30 FPS; the pause-menu save
  writes the card, and the saved card reloads with all three mods and with none (30 FPS both). The
  menu tree, checked with `BLUEWAKE_MENU_DUMP=1`, lists the Mods section and the installed pack.
- Better Wind Waker: its added code section runs (guest program-counter samples), and the Items screen
  shows its Swift Sail in place of the Sail, and the Swift Sail was used at sea on the iPad on 2026-09-28.

## Building

The Builder (`scripts/builder/build.sh`, or `scripts/ios/build_device.sh`) adds the mods itself, as its step 6;
`--no-mods` leaves them out. It installs the PyYAML and Pillow it needs into `build/python` when the Mac's Python lacks them. The step runs

```sh
scripts/mods/build_mods.sh BUILD_DIR DISC.iso
```

which applies the widescreen code, patches the disc with Better Wind Waker, translates the
widescreen, Better Wind Waker and combined executables (and Better Wind Waker's RELs) with the build's
own DolRecomp, generates a composite tree for each, and adds the variants to the build's composite
source before the compile. Copy `BUILD_DIR/mods/betterww.iso` (`build/device/mods/betterww.iso` by default)
to the iPad for Better Wind Waker.
