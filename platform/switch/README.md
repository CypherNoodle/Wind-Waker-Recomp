# Nintendo Switch bring-up

This directory contains the first Switch milestone: a devkitA64/libnx target
that compiles and links the project's pinned GXRuntime core as AArch64, then
packages the ELF as `WindWakerRecomp.nro`.

It intentionally does not initialize Aurora, Dawn, Vulkan, NVK, audio, or the
translated game yet. Those are later milestones and must not be represented by
no-op backends merely to make the link succeed.

## Requirements

- devkitPro with the `switch-dev` group (devkitA64, libnx and switch tools)
- CMake 3.25 or newer
- Ninja
- the exact RecompCore revision from `config/dependencies.lock.json`, checked
  out at `ref/recompcore` by the existing bootstrap/builder flow

## Build

```sh
export DEVKITPRO=/opt/devkitpro
platform/switch/build.sh
```

The result is the SD-card-ready directory
`build/switch/switch/WindWakerRecomp/`. The target uses devkitPro's official
`nx_generate_nacp` and `nx_create_nro` helpers and follows the packaging layout
validated by `CypherNoodle/PaperBoat-nx`. Copy that directory to `/switch/` on
the SD card.

For a RecompCore checkout elsewhere, add:

```sh
platform/switch/build.sh -DBLUEWAKE_RECOMPCORE_DIR=/path/to/RecompCore
```

The current program initializes libnx console and HID, calls the real
GXRuntime event-clock implementation, and exits with the `+` button. It is a
link/toolchain proof, not a renderer or gameplay milestone.

The build-side proof was completed with devkitA64 15.2.0 on 2026-10-02. See
`PORTING_AUDIT.md` for the pinned official toolchain-image digest, ELF details,
and NRO checksum. Running the result still requires a physical Switch in
Application mode; this repository does not treat a successful package step as
hardware validation.

## Milestone 2: NVK clear/present gate

Build and package `HayatoG/switch-nvk` at the revision recorded in
`PORTING_AUDIT.md`, then point this project at its install tree:

```sh
export DEVKITPRO=/opt/devkitpro
export BLUEWAKE_NVK_DIR=/path/to/switch-nvk/nvk-switch
platform/switch/build-vulkan-smoke.sh
```

This produces
`build/switch-vulkan/switch/WindWakerVulkanSmoke/WindWakerVulkanSmoke.nro`.
It uses a real `NWindow`, `VK_NN_vi_surface`, NVK swapchain, command buffer,
image clear, queue submission and presentation loop. A successful run cycles
the display between blue and red; press `+` to exit. Failures are written to
`sdmc:/WindWakerRecomp-vulkan-smoke.log` and Mesa diagnostics to the adjacent
`-mesa.log` file.

The target consumes NVK as an external, pinned static package. Neither the
71 MiB library nor generated Mesa sources are committed here. This gate does
not enable Aurora/Dawn and does not claim hardware success until the NRO is run
on a physical Switch in Application mode.

The build script infers the switch-nvk source directory and its `mb` cross-build
directory from `BLUEWAKE_NVK_DIR`. They can be overridden with
`BLUEWAKE_NVK_SOURCE_DIR` and `BLUEWAKE_NVK_BUILD_DIR`. The executable links
the original component archives because Mesa's weak Vulkan dispatch references
must not be resolved by selective extraction from the convenience fat archive.

## Milestone 3: Aurora/Dawn source integration

The source-side renderer path is now staged behind
`BLUEWAKE_SWITCH_ENABLE_AURORA`. It selects the libnx `NWindow` and PAD
backends from `CypherNoodle/RecompCore`, builds the pinned
`CypherNoodle/dawn-switch` source, and attaches the same component-archive NVK
recipe that passed the hardware clear/present gate.
When enabled, the application selects `source/aurora_gate.cpp`: it initializes
the real GXRuntime Aurora backend, presents empty GX frames through Dawn/NVK,
and records startup, first-present and ordered shutdown to
`sdmc:/WindWakerRecomp-aurora-gate.log`. This is the next hardware gate before
translated game code is admitted.

The exact RecompCore and Dawn revisions are recorded in
`config/dependencies.lock.json`. Check Dawn out at `ref/dawn-switch`, then a
future renderer build can be configured with:

```sh
platform/switch/build.sh \
  -DBLUEWAKE_SWITCH_ENABLE_AURORA=ON \
  -DBLUEWAKE_DAWN_DIR=/path/to/dawn-switch \
  -DBLUEWAKE_NVK_DIR=/path/to/switch-nvk/nvk-switch \
  -DBLUEWAKE_NVK_SOURCE_DIR=/path/to/switch-nvk \
  -DBLUEWAKE_NVK_BUILD_DIR=/path/to/switch-nvk/mb
```

This integration has intentionally not been compiled yet. It is a source
milestone, not a claim that Aurora or translated gameplay renders on hardware.
The default remains the verified GXRuntime bring-up target until the next build
is explicitly authorized.

## Milestone 4: statically linked game host

The source tree now stages the first complete host path behind
`BLUEWAKE_SWITCH_ENABLE_GAME`. Unlike macOS, an NRO cannot load the translated
game from a `.dylib`, so the composite can be built as a static library and its
existing exported ABI is resolved inside the executable. The desktop dynamic
loader remains available through the same resolver interface.

The Switch entry point supplies only platform defaults. At runtime it expects
the owner's files in this layout:

```text
sdmc:/switch/WindWakerRecomp/
  WindWakerRecomp.nro
  game/
    main.dol
    GZLE01.iso
    rels/
  save/
  states/
```

`main.dol`, the RELs, the disc image and the generated composite are never
committed or included in the project's public CI artifacts.
`BLUEWAKE_COMPOSITE_DIR` must point at portable C output produced locally by the
existing DolRecomp/composite flow; host-native `.o` chunks are rejected because
they cannot be linked into an AArch64 NRO. A personal full-host NRO necessarily
contains translated code derived from the owner's game and must not be
redistributed.

Once builds are authorized, the staged full-host configuration is:

```sh
platform/switch/build.sh \
  -DBLUEWAKE_SWITCH_ENABLE_AURORA=ON \
  -DBLUEWAKE_SWITCH_ENABLE_GAME=ON \
  -DBLUEWAKE_COMPOSITE_DIR=/path/to/user-generated/composite \
  -DBLUEWAKE_DAWN_DIR=/path/to/dawn-switch \
  -DBLUEWAKE_NVK_DIR=/path/to/switch-nvk/nvk-switch \
  -DBLUEWAKE_NVK_SOURCE_DIR=/path/to/switch-nvk \
  -DBLUEWAKE_NVK_BUILD_DIR=/path/to/switch-nvk/mb
```

This path reuses the portable host's MEM1/ARAM, VI clock, SI/PAD, DI/DVD,
audio-DMA and save-card services. Desktop-only ImGui, mouse-camera, jump-button
and sprint input adapters are deliberately stubbed on Switch until their UI
and controls are mapped to native libnx input. No full-host build or hardware
claim has been made yet.

## Continuous integration

`.github/workflows/switch.yml` follows the PaperBoat-nx Switch workflow: it
runs in the official `devkitpro/devkita64` container, verifies the preinstalled
tools, fetches the exact RecompCore SHA from `config/dependencies.lock.json`,
builds `switch-package`, and uploads the SD-card-ready directory. It runs only
for `switch-bringup` pushes or manual dispatches.
