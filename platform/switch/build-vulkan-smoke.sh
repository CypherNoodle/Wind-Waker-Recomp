#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${DEVKITPRO:-}" ]]; then
    echo "error: DEVKITPRO is not set" >&2
    exit 1
fi
if [[ -z "${BLUEWAKE_NVK_DIR:-}" ]]; then
    echo "error: BLUEWAKE_NVK_DIR must name the packaged switch-nvk tree" >&2
    exit 1
fi

BLUEWAKE_NVK_SOURCE_DIR="${BLUEWAKE_NVK_SOURCE_DIR:-$(cd "${BLUEWAKE_NVK_DIR}/.." && pwd)}"
BLUEWAKE_NVK_BUILD_DIR="${BLUEWAKE_NVK_BUILD_DIR:-${BLUEWAKE_NVK_SOURCE_DIR}/mb}"

export PATH="${DEVKITPRO}/devkitA64/bin:${DEVKITPRO}/tools/bin:${PATH}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${BLUEWAKE_SWITCH_BUILD_DIR:-${repo_root}/build/switch-vulkan}"

cmake -S "${repo_root}/platform/switch" -B "${build_dir}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${DEVKITPRO}/cmake/Switch.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBLUEWAKE_NVK_DIR="${BLUEWAKE_NVK_DIR}" \
    -DBLUEWAKE_NVK_SOURCE_DIR="${BLUEWAKE_NVK_SOURCE_DIR}" \
    -DBLUEWAKE_NVK_BUILD_DIR="${BLUEWAKE_NVK_BUILD_DIR}" \
    "$@"
cmake --build "${build_dir}" --target switch-vulkan-smoke-package

echo "${build_dir}/switch/WindWakerVulkanSmoke/WindWakerVulkanSmoke.nro"
