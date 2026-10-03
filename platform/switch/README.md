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

## Continuous integration

`.github/workflows/switch.yml` follows the PaperBoat-nx Switch workflow: it
runs in the official `devkitpro/devkita64` container, verifies the preinstalled
tools, fetches the exact RecompCore SHA from `config/dependencies.lock.json`,
builds `switch-package`, and uploads the SD-card-ready directory. It runs only
for `switch-bringup` pushes or manual dispatches.
