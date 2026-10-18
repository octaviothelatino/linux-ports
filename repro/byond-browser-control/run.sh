#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
byond_prefix=${BYOND_PREFIX:-/home/octavio/Downloads/linux-ports/byond32-upstream-baseline}
byond_runner=${BYOND_RUNNER:-/home/octavio/Downloads/wine-build-upstream32/install}
wine_bin=${byond_runner}/bin/wine
dreamseeker_exe=${BYOND_DREAMSEEKER_EXE:-'C:\Program Files\BYOND\bin\dreamseeker.exe'}
world_file=${project_dir}/browser-control-repro.dmb
wine_d3d_overrides=${WINEDLLOVERRIDES:-'d3d11=builtin;dxgi=builtin;d3d9=builtin;d3d12=builtin'}

if [[ ! -f "${world_file}" ]]; then
	echo "World not built: ${world_file}" >&2
	echo "Run ./build.sh first." >&2
	exit 1
fi

echo "[run] prefix=${byond_prefix}"
echo "[run] runner=${wine_bin}"
echo "[run] world=${world_file}"

WINEDLLOVERRIDES="${wine_d3d_overrides}" \
WINEPREFIX="${byond_prefix}" \
	"${wine_bin}" "${dreamseeker_exe}" "${world_file}"
