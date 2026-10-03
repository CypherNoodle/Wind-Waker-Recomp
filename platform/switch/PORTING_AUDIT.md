# Switch port audit

Audit performed on 2026-10-02. Revisions are recorded explicitly so later
graphics work can be reproduced instead of following moving branches.

## Product baseline

- Wind-Waker-Recomp: `d102695a1847963504ea55d1cf0bc96a9fa663ec`.
- RecompCore (`bluewake`): `8ab24daee9c641634fda5cac30389ad4b2cfda5e`.
- DolRecomp (`bluewake`): `b8b534591cba8ca7cd43943a655ee6e2591cf5de`.
- The product Aurora is not an independent submodule. It is an owned hard fork
  vendored in RecompCore at `GXRuntime/graphics/aurora`.
- Its documented upstream fork point is encounter/aurora
  `05495810ba4bc906f4f9a131cb5792011b3f35c4`, plus seven BlueWake bootstrap
  patches already folded into the vendor tree. Replacing this directory with a
  Switch fork would discard guest-memory resolvers, replay fixes, EFB readback,
  pipeline synchronization, and GX behavior fixes.

Conclusion: preserve the pinned BlueWake tree and port only the Switch platform
seams. Do not replace Aurora wholesale.

## Build and platform blockers found

- The repository has no cross-platform top-level application CMake target. Its
  only current GUI application target is `apple/ios/CMakeLists.txt`, which owns
  UIKit/Objective-C entry code and Apple framework links. The new Switch target
  therefore composes the already-portable GXRuntime directly instead of trying
  to conditionally mutate the iOS bundle.
- The 35-source GXRuntime C core is portable C and configures independently
  when both Aurora options are off. Its renderer path is C++20 and currently
  assumes SDL3 window/input plus the desktop/Apple Dawn platform selection.
- The vendored Aurora has no `AURORA_PLATFORM_SWITCH` selection, no libnx
  window/input implementations, no Dawn `NWindow` surface descriptor, and no
  Switch Vulkan/NVK link recipe. These are the main graphics build blockers.
- Aurora's cache uses SQLite/POSIX behavior that needs the Switch VFS or
  lock-free `unix-none` handling demonstrated by the two Switch forks. Its
  asynchronous pipeline compilation also needs an explicit, sufficiently large
  pthread stack on libnx.
- The existing app host and DSP donor are wired by the iOS target and contain
  Apple framework, Objective-C, app-container, and input/UI integration. Their
  portable pieces must be selected into a new host target; the Apple entry and
  framework sources must not be compiled on Switch.
- The composite build accepts either generated C chunks or already-generated
  native `.o` chunks. Existing LLVM objects are platform/object-format and CPU
  specific, so they cannot be linked into a Horizon AArch64 ELF. The first game
  integration should cross-compile the generated C backend with devkitA64, or
  add and validate a DolRecomp AArch64 ELF target before consuming native
  objects. Reusing macOS/iOS Mach-O objects is invalid.
- The host does not have a system-wide devkitPro installation or a Docker/
  Podman daemon. For reproducible local verification, the official
  `devkitpro/devkita64` image was pinned by digest and its filesystem was
  extracted into the workspace. This provides the unmodified devkitA64,
  libnx, `nacptool`, and `elf2nro` toolchain without requiring root access.

## Switch references inspected

| Reference | Revision | Reusable evidence |
|---|---|---|
| HayatoG/aurora-switch, `dusklight-switch-port` | `64cb652f28f13b039a81bd6b3c58c2cf73a49ef7` | Smallest clear Aurora/Dawn Switch delta: libnx window/input, `NWindow` Dawn surface, NVK static link, SQLite `unix-none`, and an 8 MiB pipeline-worker stack. |
| souldbminerr/aurora-switch, `switch` | `6d9f9d9fe8952aada5274154645610042d0a036e` | Newer but more entangled port; useful for its Switch SQLite VFS and thread/runtime tuning, not as a drop-in base. |
| HayatoG/dusklight | `95322b8616d3f18ec438ab37479bbca1d22d73a6` | End-to-end libnx lifecycle, Dawn/Vulkan/NVK wiring, packaging, cache behavior, and a hardware-tested audren backend. Some older planning files are stale; `RESUME_REPORT_V141.md` records the later working state. |
| HayatoG/switch-nvk, `master` | `6eec707da3ad5f86c64f748226583202801bfd03` | Packaging and integration source of truth. |
| HayatoG/switch-nvk, `switch-port/nvk-wsi` | `0771652cfba18279c21a0917e944123b1ef6b89b` | `VK_NN_vi_surface`/`nwindow` WSI bring-up. |
| HayatoG/switch-nvk, `switch-port/triple-buffer` | `2a454df9c7e35028258b3303f90e767194f38f31` | Three-buffer WSI variant. |
| HayatoG/switch-nvk, `switch-port/wsi-zero-copy` | `beaddd335e17d59346c7c96d6c7baeb29e674481` | Block-linear zero-copy present path. |
| danfromtico/mesa-switch | `d4a00ea0ab3f59afb967cc5d779e4263d237bd77` | Mesa 26.2.3 Switch baseline used only as a lower-layer reference. |
| CypherNoodle/PaperBoat-nx, `switch-port` | `9b4d3381dc8551fe124c7596ca912d0e24530891` | Owner's proven devkitA64 CMake/package/CI pattern: official container, toolchain verification, `nx_create_nro`, SD-card tree, and artifact upload. |

## Graphics integration decision

The minimal path is BlueWake Aurora -> Dawn Vulkan -> packaged switch-nvk ->
libnx `nwindow`. The first transplant should be the platform boundary from
HayatoG's Aurora port: `AURORA_PLATFORM_SWITCH`, libnx window/input sources,
`SurfaceSourceSwitchNWindow`, Switch Dawn configuration, and explicit NVK link
inputs. The BlueWake renderer and GXRuntime interfaces remain authoritative.

NVK must be consumed as a pinned install tree, not rebuilt implicitly inside
the application build. Its public package combines the Vulkan driver, WSI, and
compatibility shims; its consumer recipe also requires the documented wrapped
POSIX calls and static dependencies. Zero-copy and triple buffering are later
performance choices, not prerequisites for the first clear/present frame.

## Audio decision (documented before modification)

No audio code is changed in milestone 1. GXRuntime already owns the emulated
DSP/audio production path; the Switch-specific missing piece is an output sink.
Dusklight provides the closest validated pattern: libnx `audren`, a dedicated
worker, four queued wave buffers, `armDCacheFlush` before submission, and
float32-stereo to signed-16 conversion because its tested audren voice rejected
`PcmFormat_Float`.

When audio work begins, keep GXRuntime's DSP and mixer intact and implement a
small output backend at its existing host-audio boundary. Do not import
Dusklight's game audio subsystem or replace the DSP. Validate initialization,
buffer underruns, sample rate, pause/resume, and shutdown on hardware before
enabling it by default.

## Milestone 1 verification

The toolchain gate passed on 2026-10-02 using the official image
`devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`
and `aarch64-none-elf-gcc (devkitA64) 15.2.0`.

- `WindWakerRecomp.elf`: 2,950,320 bytes, ELF64 AArch64 static PIE, build ID
  `5d368d80fbe16c7c18b02ac1ca6bafeae5b030c1`.
- `WindWakerRecomp.nro`: 215,237 bytes.
- NRO SHA-256:
  `fb7e6d5000bd859b0cd5bf7dcdc6b785439c141a46955ffdb3623515d64fd1f0`.
- The packaged output is
  `build/switch/switch/WindWakerRecomp/WindWakerRecomp.nro`.

This completes the build-side portion of milestone 1. Launch and input/exit
behavior still require physical Switch validation. Milestone 2 may now add a
standalone libnx/NWindow/Vulkan/NVK clear-and-present test. Aurora, Dawn,
translated game objects, and audio remain gated until that graphics test is
both built and hardware-validated. No no-op render/audio implementations may
be added to bypass either gate.

## Milestone 2 build-side verification

The pinned `HayatoG/switch-nvk` source was rebuilt locally against Mesa 25.0.7
with devkitA64 15.2.0. Its static NVK, NIR/SPIR-V compiler, Nouveau winsys,
`VK_NN_vi_surface` WSI, compatibility shims and Vulkan runtime all compiled for
AArch64. Mesa's final shared-library target is intentionally inapplicable to
Horizon/libnx; the application consumes the completed static archives instead.

The repository target `WindWakerVulkanSmoke` then linked against the packaged
NVK tree and produced a real NRO on 2026-10-02:

- Corrected ELF: 23,855,144 bytes, ELF64 AArch64 PIE, build ID
  `33e011c909f416cfea4ce5641817ef45af717d10`.
- Corrected NRO: 12,671,173 bytes, SHA-256
  `edf89427995d3fa1513a86aee928e252fbb7ff20cf1e7774d7427ead35c2386d`.
- The linked ELF defines `vk_icdGetInstanceProcAddr`,
  `wsi_CreateViSurfaceNN`, and `wsi_switch_init_wsi`.

This completes only the build/link/package portion of milestone 2. The NRO
must still be run on physical hardware and show the cycling clear colour before
Aurora/Dawn work begins.

The first hardware run reached `vkEnumeratePhysicalDevices` and aborted at a
null `vkGetPhysicalDeviceProperties2` dispatch pointer in `wsi_device_init`.
The cause was selective extraction from switch-nvk's convenience fat archive:
weakly referenced Vulkan entrypoints were omitted. The consumer now follows
the reference executable recipe exactly: whole-archive only for `libnvk.a`,
then the original support archives inside a linker group. Hardware validation
must use the rebuilt artifact containing this correction.
