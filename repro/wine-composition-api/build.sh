#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir=${root_dir}/build
cc=${MINGW32_CC:-i686-w64-mingw32-gcc}

mkdir -p "${build_dir}"

"${cc}" \
    -std=c11 \
    -O2 \
    -Wall \
    -Wextra \
    -D_WIN32_WINNT=0x0603 \
    "${root_dir}/composition-api.c" \
    -o "${build_dir}/wine-composition-api.exe" \
    -ld3d11 -ldxgi -ldcomp -lole32 -luser32 -luuid

echo "built ${build_dir}/wine-composition-api.exe"
