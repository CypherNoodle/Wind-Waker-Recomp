cmake_minimum_required(VERSION 3.25)

project(WindWakerRecompSwitch LANGUAGES C)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch" OR
   NOT CMAKE_C_COMPILER MATCHES "aarch64-none-elf")
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
