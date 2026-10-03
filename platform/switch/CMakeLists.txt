cmake_minimum_required(VERSION 3.25)

project(WindWakerRecompSwitch LANGUAGES C CXX)

if(NOT NINTENDO_SWITCH OR NOT COMMAND nx_create_nro)
    message(FATAL_ERROR
        "The Switch target must be cross-compiled with devkitA64. Configure with "
        "-DCMAKE_TOOLCHAIN_FILE=$ENV{DEVKITPRO}/cmake/Switch.cmake")
endif()

if(NOT DEFINED ENV{DEVKITPRO} OR NOT EXISTS "$ENV{DEVKITPRO}/libnx/include/switch.h")
    message(FATAL_ERROR
        "libnx was not found. Install the devkitPro switch-dev group and set DEVKITPRO.")
endif()

set(BLUEWAKE_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(BLUEWAKE_RECOMPCORE_DIR "${BLUEWAKE_REPO_ROOT}/ref/recompcore"
    CACHE PATH "RecompCore checkout pinned by config/dependencies.lock.json")
set(BLUEWAKE_DAWN_DIR "${BLUEWAKE_REPO_ROOT}/ref/dawn-switch"
    CACHE PATH "dawn-switch checkout pinned by config/dependencies.lock.json")
set(BLUEWAKE_NVK_DIR "" CACHE PATH
    "switch-nvk install tree containing include/ and lib/libvulkan.a")
set(BLUEWAKE_NVK_SOURCE_DIR "" CACHE PATH
    "switch-nvk source tree used to build the pinned NVK package")
set(BLUEWAKE_NVK_BUILD_DIR "" CACHE PATH
    "Mesa cross-build directory produced by switch-nvk")
option(BLUEWAKE_SWITCH_ENABLE_AURORA
    "Stage the Aurora -> Dawn -> NVK renderer in the application target" OFF)
option(BLUEWAKE_SWITCH_ENABLE_GAME
    "Statically link the translated game and portable BlueWake host" OFF)
set(BLUEWAKE_COMPOSITE_DIR "" CACHE PATH
    "External generated composite made from the user's GZLE01 disc")

if(BLUEWAKE_SWITCH_ENABLE_GAME AND NOT BLUEWAKE_SWITCH_ENABLE_AURORA)
    message(FATAL_ERROR
        "BLUEWAKE_SWITCH_ENABLE_GAME requires BLUEWAKE_SWITCH_ENABLE_AURORA")
endif()

if(NOT EXISTS "${BLUEWAKE_RECOMPCORE_DIR}/GXRuntime/CMakeLists.txt")
    message(FATAL_ERROR
        "Pinned RecompCore checkout not found at ${BLUEWAKE_RECOMPCORE_DIR}. "
        "Run scripts/bootstrap.sh first or set BLUEWAKE_RECOMPCORE_DIR.")
endif()

# Milestone 1 deliberately builds the real portable GXRuntime core, but not the
# Aurora/Dawn renderer. Graphics is enabled only after the base AArch64 ELF/NRO
# path has been verified with the Switch toolchain.
set(BUILD_TESTING OFF CACHE BOOL "Do not cross-build host tests" FORCE)
if(BLUEWAKE_SWITCH_ENABLE_AURORA)
    include(cmake/AuroraBringup.cmake)
    bluewake_configure_switch_aurora()
else()
    set(GXRUNTIME_ENABLE_AURORA OFF CACHE BOOL "Renderer is opt-in during bring-up" FORCE)
    set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "Renderer frontend is opt-in during bring-up" FORCE)
endif()
add_subdirectory("${BLUEWAKE_RECOMPCORE_DIR}/GXRuntime" gxruntime)

if(BLUEWAKE_SWITCH_ENABLE_GAME)
    if(NOT EXISTS "${BLUEWAKE_COMPOSITE_DIR}/generated_composite.h" OR
       NOT EXISTS "${BLUEWAKE_COMPOSITE_DIR}/module_tables.inc" OR
       NOT EXISTS "${BLUEWAKE_COMPOSITE_DIR}/rel_modules.inc")
        message(FATAL_ERROR
            "BLUEWAKE_COMPOSITE_DIR must be a generated composite prepared "
            "from the user's supported GZLE01 disc")
    endif()
    file(GLOB BLUEWAKE_COMPOSITE_NATIVE_OBJECTS
        "${BLUEWAKE_COMPOSITE_DIR}/chunks_dol/*.o")
    if(BLUEWAKE_COMPOSITE_NATIVE_OBJECTS)
        message(FATAL_ERROR
            "The Switch target requires portable DolRecomp C output; the "
            "selected composite contains host-native DOL objects")
    endif()

    set(COMPOSITE_DIR "${BLUEWAKE_COMPOSITE_DIR}" CACHE PATH "" FORCE)
    set(GXRUNTIME_DIR "${BLUEWAKE_RECOMPCORE_DIR}/GXRuntime" CACHE PATH "" FORCE)
    set(ABI_DIR
        "${BLUEWAKE_RECOMPCORE_DIR}/Source/Core/Core/PowerPC/StaticRecomp"
        CACHE PATH "" FORCE)
    set(BLUEWAKE_COMPOSITE_STATIC ON CACHE BOOL "" FORCE)
    add_subdirectory("${BLUEWAKE_REPO_ROOT}/cmake/composite"
        bluewake_composite)

    set(BLUEWAKE_HOST_SRC "${BLUEWAKE_REPO_ROOT}/runtime/host/src")
    set(BLUEWAKE_SWITCH_MAIN
        source/switch_entry.cpp
        "${BLUEWAKE_HOST_SRC}/main.c"
        "${BLUEWAKE_HOST_SRC}/module_symbols.c"
        "${BLUEWAKE_HOST_SRC}/aram_dma.c"
        "${BLUEWAKE_HOST_SRC}/audio_capture.c"
        "${BLUEWAKE_HOST_SRC}/ipl_sram.c"
        "${BLUEWAKE_HOST_SRC}/card_runtime.c"
        "${BLUEWAKE_HOST_SRC}/callback_delivery.c"
        "${BLUEWAKE_HOST_SRC}/cycle_domain.c"
        "${BLUEWAKE_HOST_SRC}/delivery_digest.c"
        "${BLUEWAKE_HOST_SRC}/edge_intercepts.c"
        "${BLUEWAKE_HOST_SRC}/game_options.c"
        "${BLUEWAKE_HOST_SRC}/fast_load.c"
        "${BLUEWAKE_HOST_SRC}/fps_watch.c"
        "${BLUEWAKE_HOST_SRC}/climb.c"
        "${BLUEWAKE_HOST_SRC}/quick_doors.c"
        "${BLUEWAKE_HOST_SRC}/draw_tags.c"
        "${BLUEWAKE_HOST_SRC}/return_census.c"
        "${BLUEWAKE_HOST_SRC}/interrupt_sources.c"
        "${BLUEWAKE_HOST_SRC}/cold_fallback.c"
        "${BLUEWAKE_HOST_SRC}/external_memory.c"
        "${BLUEWAKE_HOST_SRC}/fpu_context.c"
        "${BLUEWAKE_HOST_SRC}/pad_event_schedule.c"
        "${BLUEWAKE_HOST_SRC}/pad_waypoint.c"
        "${BLUEWAKE_HOST_SRC}/pad_wire.c"
        "${BLUEWAKE_HOST_SRC}/rel_scratch_allocator.c"
        "${BLUEWAKE_HOST_SRC}/scheduler_contract.c"
        "${BLUEWAKE_HOST_SRC}/save_state.c")
elseif(BLUEWAKE_SWITCH_ENABLE_AURORA)
    set(BLUEWAKE_SWITCH_MAIN source/aurora_gate.cpp)
else()
    set(BLUEWAKE_SWITCH_MAIN source/main.c)
endif()
add_executable(WindWakerRecomp ${BLUEWAKE_SWITCH_MAIN})
if(BLUEWAKE_SWITCH_ENABLE_AURORA)
    target_compile_features(WindWakerRecomp PRIVATE cxx_std_20)
else()
    target_compile_features(WindWakerRecomp PRIVATE c_std_11)
endif()
target_compile_options(WindWakerRecomp PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -ffunction-sections
    -fdata-sections)
target_link_options(WindWakerRecomp PRIVATE -Wl,--gc-sections)
target_link_libraries(WindWakerRecomp PRIVATE GXRuntime::runtime nx)
if(BLUEWAKE_SWITCH_ENABLE_AURORA)
    target_link_libraries(WindWakerRecomp PRIVATE GXRuntime::aurora)
    bluewake_link_switch_nvk(WindWakerRecomp)
endif()
if(BLUEWAKE_SWITCH_ENABLE_GAME)
    set_source_files_properties("${BLUEWAKE_HOST_SRC}/main.c" PROPERTIES
        COMPILE_DEFINITIONS "main=bluewake_host_main")
    target_compile_definitions(WindWakerRecomp PRIVATE
        BLUEWAKE_STATIC_MODULE=1
        BLUEWAKE_ENABLE_DEVELOPER_TRACING=0
        BLUEWAKE_EDGE_CENSUS=0
        DOLRECOMP_CPU_HEADER="core/cpu.h")
    target_include_directories(WindWakerRecomp PRIVATE
        "${BLUEWAKE_HOST_SRC}"
        "${BLUEWAKE_RECOMPCORE_DIR}/GXRuntime/include"
        "${ABI_DIR}")
    # Repeat the runtime and libnx after the composite: both are static
    # archives, and generated/module glue introduces CPU/libnx references
    # after their first occurrence on the link line.
    target_link_libraries(WindWakerRecomp PRIVATE
        gGZLE01_recomp GXRuntime::runtime nx z m pthread)
    set_target_properties(WindWakerRecomp PROPERTIES LINKER_LANGUAGE CXX)
endif()

set(BLUEWAKE_NACP "${CMAKE_CURRENT_BINARY_DIR}/WindWakerRecomp.nacp")
set(BLUEWAKE_NRO "${CMAKE_CURRENT_BINARY_DIR}/WindWakerRecomp.nro")

nx_generate_nacp(
    OUTPUT "${BLUEWAKE_NACP}"
    NAME "Wind Waker Recomp"
    AUTHOR "Wind Waker Recomp contributors"
    VERSION "0.1.0")
nx_create_nro(WindWakerRecompNro
    TARGET WindWakerRecomp
    OUTPUT "${BLUEWAKE_NRO}"
    NACP "${BLUEWAKE_NACP}")

# Match the SD-card-ready artifact layout proven by PaperBoat-nx. This package
# contains only the homebrew executable; game data must be supplied by the user.
set(BLUEWAKE_SWITCH_PACKAGE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/switch/WindWakerRecomp")
add_custom_target(switch-package
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${BLUEWAKE_SWITCH_PACKAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${BLUEWAKE_NRO}"
        "${BLUEWAKE_SWITCH_PACKAGE_DIR}/WindWakerRecomp.nro"
    DEPENDS WindWakerRecompNro
    VERBATIM)

# Milestone 2 is deliberately a standalone renderer gate. It validates the
# libnx NWindow -> VK_NN_vi_surface -> NVK swapchain path before Aurora/Dawn is
# admitted to the application target. The large NVK build remains external.
if(BLUEWAKE_NVK_DIR)
    if(NOT EXISTS "${BLUEWAKE_NVK_DIR}/lib/libvulkan.a" OR
       NOT EXISTS "${BLUEWAKE_NVK_DIR}/include/vulkan/vulkan.h")
        message(FATAL_ERROR
            "BLUEWAKE_NVK_DIR must point to a packaged switch-nvk install tree")
    endif()
    if(NOT EXISTS "${BLUEWAKE_NVK_SOURCE_DIR}/winsys/drm_shim.c" OR
       NOT EXISTS "${BLUEWAKE_NVK_SOURCE_DIR}/compat/compat.c")
        message(FATAL_ERROR
            "BLUEWAKE_NVK_SOURCE_DIR must point to the pinned switch-nvk source tree")
    endif()
    if(NOT EXISTS "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/vulkan/libnvk.a")
        message(FATAL_ERROR
            "BLUEWAKE_NVK_BUILD_DIR must point to its completed Mesa cross-build")
    endif()

    add_executable(WindWakerVulkanSmoke
        source/vulkan_smoke.c
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys/drm_shim.c"
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys/switch_libc_shim.c"
        "${BLUEWAKE_NVK_SOURCE_DIR}/compat/compat.c")
    target_compile_features(WindWakerVulkanSmoke PRIVATE c_std_11)
    target_compile_definitions(WindWakerVulkanSmoke PRIVATE
        __SWITCH__ _GNU_SOURCE)
    target_include_directories(WindWakerVulkanSmoke PRIVATE
        "${BLUEWAKE_NVK_DIR}/include"
        "${BLUEWAKE_NVK_SOURCE_DIR}/switch-cross-include"
        "${BLUEWAKE_NVK_SOURCE_DIR}/mesa-25/include"
        "${BLUEWAKE_NVK_SOURCE_DIR}/mesa-25/src/nouveau/drm"
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys"
        "${BLUEWAKE_NVK_SOURCE_DIR}/compat")
    target_compile_options(WindWakerVulkanSmoke PRIVATE
        -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
        -include "${BLUEWAKE_NVK_SOURCE_DIR}/compat/switch_compat.h")
    target_link_options(WindWakerVulkanSmoke PRIVATE
        -Wl,--gc-sections
        -Wl,--wrap=open
        -Wl,--wrap=close
        -Wl,--wrap=stat
        -Wl,--wrap=lstat)
    target_link_libraries(WindWakerVulkanSmoke PRIVATE
        -Wl,--whole-archive
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/vulkan/libnvk.a"
        -Wl,--no-whole-archive
        -Wl,--start-group
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/codegen/libnouveau_codegen.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/util/libmesa_util.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/util/libmesa_util_sse41.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/util/blake3/libblake3.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/c11/impl/libmesa_util_c11.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/compiler/libnak.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/compiler/libnak_rs.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/compiler/rust/libcompiler_c_helpers.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/headers/libnvidia_headers_c.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/nil/liblibnil.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/nil/liblibnil_format_table.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/compiler/nir/libnir.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/compiler/libcompiler.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/mme/libnouveau_mme.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/winsys/libnouveau_ws.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/vulkan/util/libvulkan_util.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/compiler/spirv/libvtn.a"
        "${BLUEWAKE_NVK_BUILD_DIR}/src/util/libxmlconfig.a"
        nx z zstd expat m pthread
        -Wl,--end-group)
    set_target_properties(WindWakerVulkanSmoke PROPERTIES LINKER_LANGUAGE CXX)

    set(BLUEWAKE_VULKAN_NACP
        "${CMAKE_CURRENT_BINARY_DIR}/WindWakerVulkanSmoke.nacp")
    set(BLUEWAKE_VULKAN_NRO
        "${CMAKE_CURRENT_BINARY_DIR}/WindWakerVulkanSmoke.nro")
    nx_generate_nacp(
        OUTPUT "${BLUEWAKE_VULKAN_NACP}"
        NAME "Wind Waker NVK Bring-up"
        AUTHOR "Wind Waker Recomp contributors"
        VERSION "0.2.0")
    nx_create_nro(WindWakerVulkanSmokeNro
        TARGET WindWakerVulkanSmoke
        OUTPUT "${BLUEWAKE_VULKAN_NRO}"
        NACP "${BLUEWAKE_VULKAN_NACP}")

    set(BLUEWAKE_VULKAN_PACKAGE_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/switch/WindWakerVulkanSmoke")
    add_custom_target(switch-vulkan-smoke-package
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${BLUEWAKE_VULKAN_PACKAGE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${BLUEWAKE_VULKAN_NRO}"
            "${BLUEWAKE_VULKAN_PACKAGE_DIR}/WindWakerVulkanSmoke.nro"
        DEPENDS WindWakerVulkanSmokeNro
        VERBATIM)
endif()
