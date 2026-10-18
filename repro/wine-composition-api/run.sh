#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
prefix=${BYOND_PREFIX:-/home/octavio/Downloads/linux-ports/byond32-upstream-baseline}
runner=${BYOND_RUNNER:-/home/octavio/Downloads/wine-build-upstream32/install}
wine=${runner}/bin/wine

if [[ ! -x "${wine}" ]]; then
    echo "Wine runner not found: ${wine}" >&2
    exit 1
fi

if [[ ! -f "${root_dir}/build/wine-composition-api.exe" ]]; then
    "${root_dir}/build.sh"
fi

echo "prefix=${prefix}"
echo "runner=${wine}"
echo "probe=${root_dir}/build/wine-composition-api.exe"

WINEPREFIX="${prefix}" \
    "${wine}" "${root_dir}/build/wine-composition-api.exe"
