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

The result is `build/switch/WindWakerRecomp.nro`. The target uses devkitPro's
official `nx_generate_nacp` and `nx_create_nro` helpers. Copy it to
`/switch/WindWakerRecomp/WindWakerRecomp.nro` on the SD card.

For a RecompCore checkout elsewhere, add:

```sh
platform/switch/build.sh -DBLUEWAKE_RECOMPCORE_DIR=/path/to/RecompCore
```

The current program initializes libnx console and HID, calls the real
GXRuntime event-clock implementation, and exits with the `+` button. It is a
link/toolchain proof, not a renderer or gameplay milestone.
