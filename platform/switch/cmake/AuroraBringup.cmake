include_guard(GLOBAL)

function(bluewake_configure_switch_aurora)
    foreach(required_var IN ITEMS
            BLUEWAKE_DAWN_DIR
            BLUEWAKE_NVK_DIR
            BLUEWAKE_NVK_SOURCE_DIR
            BLUEWAKE_NVK_BUILD_DIR)
        if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
            message(FATAL_ERROR
                "${required_var} is required when BLUEWAKE_SWITCH_ENABLE_AURORA=ON")
        endif()
    endforeach()

    if(NOT EXISTS "${BLUEWAKE_DAWN_DIR}/src/dawn/native/Surface.cpp" OR
       NOT EXISTS "${BLUEWAKE_DAWN_DIR}/src/dawn/native/vulkan/SwapChainVk.cpp")
        message(FATAL_ERROR
            "BLUEWAKE_DAWN_DIR must point to CypherNoodle/dawn-switch at the pinned switch-bringup SHA")
    endif()
    if(NOT EXISTS "${BLUEWAKE_NVK_DIR}/include/vulkan/vulkan.h" OR
       NOT EXISTS "${BLUEWAKE_NVK_BUILD_DIR}/src/nouveau/vulkan/libnvk.a")
        message(FATAL_ERROR "The pinned switch-nvk headers and component build are required")
    endif()

    set(GXRUNTIME_ENABLE_AURORA ON CACHE BOOL "Build the Aurora Switch renderer" FORCE)
    set(GXRUNTIME_ENABLE_AURORA_RECOMP ON CACHE BOOL "Build the recomp GX frontend" FORCE)
    set(AURORA_PLATFORM_SWITCH ON CACHE BOOL "Use libnx NWindow" FORCE)
    set(AURORA_DAWN_PROVIDER source CACHE STRING "Use pinned Switch Dawn source" FORCE)
    set(AURORA_DAWN_SOURCE_DIR "${BLUEWAKE_DAWN_DIR}" CACHE PATH "Pinned Switch Dawn source" FORCE)
    set(AURORA_DAWN_LINKAGE static CACHE STRING "Static Dawn for NRO" FORCE)
    set(AURORA_ENABLE_GX ON CACHE BOOL "Enable the GX/WebGPU renderer" FORCE)
    set(AURORA_ENABLE_IMGUI OFF CACHE BOOL "Disable desktop ImGui integration" FORCE)
    set(AURORA_ENABLE_GPU_CACHE OFF CACHE BOOL "Defer the SQLite cache until its Switch VFS is enabled" FORCE)
    set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "UI integration is a later Switch milestone" FORCE)
    set(AURORA_ENABLE_DVD OFF CACHE BOOL "Disc access is owned by the host" FORCE)
    set(AURORA_ENABLE_CARD OFF CACHE BOOL "Memory card access is owned by GXRuntime" FORCE)

    add_compile_definitions(__SWITCH__ NX VK_USE_PLATFORM_VI_NN)
    include_directories(BEFORE SYSTEM "${BLUEWAKE_NVK_DIR}/include")
endfunction()

function(bluewake_link_switch_nvk target)
    target_sources(${target} PRIVATE
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys/drm_shim.c"
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys/switch_libc_shim.c"
        "${BLUEWAKE_NVK_SOURCE_DIR}/compat/compat.c")
    target_include_directories(${target} PRIVATE
        "${BLUEWAKE_NVK_DIR}/include"
        "${BLUEWAKE_NVK_SOURCE_DIR}/switch-cross-include"
        "${BLUEWAKE_NVK_SOURCE_DIR}/mesa-25/include"
        "${BLUEWAKE_NVK_SOURCE_DIR}/mesa-25/src/nouveau/drm"
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys"
        "${BLUEWAKE_NVK_SOURCE_DIR}/compat")
    set_source_files_properties(
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys/drm_shim.c"
        "${BLUEWAKE_NVK_SOURCE_DIR}/winsys/switch_libc_shim.c"
        "${BLUEWAKE_NVK_SOURCE_DIR}/compat/compat.c"
        PROPERTIES COMPILE_OPTIONS "-include;${BLUEWAKE_NVK_SOURCE_DIR}/compat/switch_compat.h")
    target_link_options(${target} PRIVATE
        -Wl,--gc-sections
        -Wl,--wrap=open
        -Wl,--wrap=close
        -Wl,--wrap=stat
        -Wl,--wrap=lstat)
    target_link_libraries(${target} PRIVATE
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
endfunction()
