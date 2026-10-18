#!/usr/bin/env bash

# Normal play with the accepted runner and prefix, without the test harness.
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${BYOND_PREFIX:-$PROJECT_ROOT/byond32-upstream-baseline}"
RUNNER="${BYOND_RUNNER:-/home/octavio/Downloads/wine-build-upstream32/install}"
PROFILE="${BYOND_WEBVIEW_PROFILE:-C:\\users\\octavio\\Documents\\BYOND\\cache\\WebView2\\WineTest\\20261014-185951-game}"
GAME_URL="${1:-byond://198.166.109.89:3336}"
TARGET="$PREFIX/drive_c/Program Files/BYOND/bin/dreamseeker.exe"

if [[ $# -gt 1 || "$GAME_URL" != byond://* ]]; then
    printf 'Usage: bash tools/play-soa.sh [byond://server:port]\n' >&2
    exit 2
fi
if [[ ! -x "$RUNNER/bin/wine" || ! -f "$TARGET" ]]; then
    printf 'The accepted Wine runner or DreamSeeker executable is missing.\n' >&2
    exit 1
fi
if pgrep -x dreamseeker.exe >/dev/null; then
    printf 'DreamSeeker is already running. Close it before starting another session.\n' >&2
    exit 1
fi

# Do not change fonts, registry settings, power policy, or graphics backends.
# BYOND may still maintain its own internal files independently of this launcher.
exec env \
    -u WEBVIEW2_PIPE_FOR_SCRIPT_DEBUGGER \
    -u WEBVIEW2_WAIT_FOR_SCRIPT_DEBUGGER \
    -u CHROME_LOG_FILE \
    WINEPREFIX="$PREFIX" \
    WINEARCH=win32 \
    WINEDEBUG=-all \
    GDK_BACKEND=x11 \
    SDL_VIDEODRIVER=x11 \
    XDG_SESSION_TYPE=x11 \
    WINEDLLOVERRIDES='d3d11=builtin;dxgi=builtin;d3d9=builtin;d3d12=builtin' \
    WEBVIEW2_USER_DATA_FOLDER="$PROFILE" \
    WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--disable-logging \
    "$RUNNER/bin/wine" "$TARGET" "$GAME_URL" \
    </dev/null >/dev/null 2>&1
