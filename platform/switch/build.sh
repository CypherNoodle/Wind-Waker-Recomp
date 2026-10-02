#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${DEVKITPRO:-}" ]]; then
    echo "error: DEVKITPRO is not set" >&2
    exit 1
fi

export PATH="${DEVKITPRO}/devkitA64/bin:${DEVKITPRO}/tools/bin:${PATH}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${BLUEWAKE_SWITCH_BUILD_DIR:-${repo_root}/build/switch}"

cmake -S "${repo_root}/platform/switch" -B "${build_dir}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${DEVKITPRO}/cmake/Switch.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    "$@"
cmake --build "${build_dir}" --target switch-package

echo "${build_dir}/switch/WindWakerRecomp/WindWakerRecomp.nro"
