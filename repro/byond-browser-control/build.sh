#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
byond_prefix=${BYOND_PREFIX:-/home/octavio/Downloads/linux-ports/byond32-upstream-baseline}
byond_runner=${BYOND_RUNNER:-/home/octavio/Downloads/wine-build-upstream32/install}
wine_bin=${byond_runner}/bin/wine
dm_exe=${BYOND_DM_EXE:-'C:\Program Files\BYOND\bin\dm.exe'}
project_file=${project_dir}/browser-control-repro.dme

if [[ ! -x "${wine_bin}" ]]; then
	echo "Wine runner not found: ${wine_bin}" >&2
	exit 1
fi

if [[ ! -f "${project_file}" ]]; then
	echo "Project file not found: ${project_file}" >&2
	exit 1
fi

echo "[build] prefix=${byond_prefix}"
echo "[build] runner=${wine_bin}"
echo "[build] project=${project_file}"

WINEPREFIX="${byond_prefix}" \
	"${wine_bin}" "${dm_exe}" "${project_file}"

test -f "${project_dir}/browser-control-repro.dmb"
test -f "${project_dir}/browser-control-repro.rsc"
echo "[build] produced browser-control-repro.dmb and browser-control-repro.rsc"
