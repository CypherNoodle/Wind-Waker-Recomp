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

if(NOT EXISTS "${BLUEWAKE_RECOMPCORE_DIR}/GXRuntime/CMakeLists.txt")
    message(FATAL_ERROR
        "Pinned RecompCore checkout not found at ${BLUEWAKE_RECOMPCORE_DIR}. "
        "Run scripts/bootstrap.sh first or set BLUEWAKE_RECOMPCORE_DIR.")
endif()

# Milestone 1 deliberately builds the real portable GXRuntime core, but not the
# Aurora/Dawn renderer. Graphics is enabled only after the base AArch64 ELF/NRO
# path has been verified with the Switch toolchain.
set(BUILD_TESTING OFF CACHE BOOL "Do not cross-build host tests" FORCE)
set(GXRUNTIME_ENABLE_AURORA OFF CACHE BOOL "Renderer is a later milestone" FORCE)
set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "Renderer frontend is a later milestone" FORCE)
add_subdirectory("${BLUEWAKE_RECOMPCORE_DIR}/GXRuntime" gxruntime)

add_executable(WindWakerRecomp source/main.c)
target_compile_features(WindWakerRecomp PRIVATE c_std_11)
target_compile_options(WindWakerRecomp PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -ffunction-sections
    -fdata-sections)
target_link_options(WindWakerRecomp PRIVATE -Wl,--gc-sections)
target_link_libraries(WindWakerRecomp PRIVATE GXRuntime::runtime nx)

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
set(BLUEWAKE_NVK_DIR "" CACHE PATH
    "switch-nvk install tree containing include/ and lib/libvulkan.a")
if(BLUEWAKE_NVK_DIR)
    if(NOT EXISTS "${BLUEWAKE_NVK_DIR}/lib/libvulkan.a" OR
       NOT EXISTS "${BLUEWAKE_NVK_DIR}/include/vulkan/vulkan.h")
        message(FATAL_ERROR
            "BLUEWAKE_NVK_DIR must point to a packaged switch-nvk install tree")
    endif()

    add_executable(WindWakerVulkanSmoke source/vulkan_smoke.c)
    target_compile_features(WindWakerVulkanSmoke PRIVATE c_std_11)
    target_compile_definitions(WindWakerVulkanSmoke PRIVATE
        __SWITCH__ _GNU_SOURCE)
    target_include_directories(WindWakerVulkanSmoke PRIVATE
        "${BLUEWAKE_NVK_DIR}/include")
    target_compile_options(WindWakerVulkanSmoke PRIVATE
        -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections)
    target_link_options(WindWakerVulkanSmoke PRIVATE
        -Wl,--gc-sections
        -Wl,--wrap=open
        -Wl,--wrap=close
        -Wl,--wrap=stat
        -Wl,--wrap=lstat)
    target_link_libraries(WindWakerVulkanSmoke PRIVATE
        "${BLUEWAKE_NVK_DIR}/lib/libvulkan.a"
        nx z zstd expat m pthread)
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
