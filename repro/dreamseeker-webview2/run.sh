#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_DIR=$(cd "$ROOT_DIR/../.." && pwd)
PREFIX=${BYOND_PREFIX:-/home/octavio/Downloads/linux-ports/byond32-upstream-baseline}
RUNNER=${BYOND_RUNNER:-/home/octavio/Downloads/wine-build-upstream32/install}
WINE="$RUNNER/bin/wine"
WINEPATH="$RUNNER/bin/winepath"

if [[ ! -x "$WINE" ]]; then
    echo "Wine runner not found: $WINE" >&2
    exit 1
fi

"$ROOT_DIR/build.sh"

LOADER_LINUX="$PREFIX/drive_c/Program Files/BYOND/bin/WebView2Loader.dll"
if [[ ! -f "$LOADER_LINUX" ]]; then
    echo "WebView2Loader.dll not found: $LOADER_LINUX" >&2
    exit 1
fi

USER_DATA_LINUX=${BYOND_REPRO_USER_DATA:-$PREFIX/drive_c/users/octavio/Documents/BYOND/cache/WebView2/DreamSeekerReproducer}
mkdir -p "$USER_DATA_LINUX"

LOADER_WIN=$("$WINEPATH" -w "$LOADER_LINUX")
USER_DATA_WIN=$("$WINEPATH" -w "$USER_DATA_LINUX")

SERVER_STATE=$(mktemp -d /tmp/dreamseeker-webview2-server.XXXXXX)
SERVER_LOG="$SERVER_STATE/server.log"
cleanup_server()
{
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$SERVER_STATE"
}
trap cleanup_server EXIT INT TERM

python3 "$ROOT_DIR/server.py" \
    --root "$ROOT_DIR/pages" \
    --port-file "$SERVER_STATE/port" \
    >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
for attempt in $(seq 1 50); do
    [[ -s "$SERVER_STATE/port" ]] && break
    sleep 0.1
done
if [[ ! -s "$SERVER_STATE/port" ]]; then
    echo "Local WebView2 HTTP server failed to start; see $SERVER_LOG" >&2
    exit 1
fi
SERVER_PORT=$(<"$SERVER_STATE/port")
BASE_URL="http://127.0.0.1:${SERVER_PORT}/"

export WINEPREFIX="$PREFIX"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-d3d11=builtin;dxgi=builtin;d3d9=builtin;d3d12=builtin}"
export GDK_BACKEND="${GDK_BACKEND:-x11}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
export XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-x11}"
if [[ ${BYOND_WEBVIEW_ARGS+x} ]]; then
    # An explicitly empty value is intentional: it exercises WebView2's
    # normal D3D11/DirectComposition path instead of the legacy fallback.
    export WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS="$BYOND_WEBVIEW_ARGS"
else
    export WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS="--disable-gpu --disable-gpu-sandbox --disable-gpu-compositing --disable-d3d11 --disable-dwm-composition --use-angle=d3d9 --disable-background-networking --disable-metrics --disable-metrics-repo --disable-crash-reporter --disable-component-update --disable-features=msEdgeMetricsReporting,msWebOOUI"
fi
export WINEDEBUG="${WINEDEBUG:--all}"

echo "prefix=$WINEPREFIX"
echo "runner=$RUNNER"
echo "loader=$LOADER_WIN"
echo "user_data=$USER_DATA_WIN"
echo "base_url=$BASE_URL"
echo "webview_args=$WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"
echo ""
echo "The map is the animated D3D9 child window. One short-lived ad WebView and"
echo "four game WebView2 documents are loaded from the local HTTP server. The ad"
echo "hides after navigation while its controller/renderer remains alive."
echo "Game bounds are 1366x696, centered 1258x696, 1x1, and 24x24."
echo "After all pages report success, move the mouse across the embedded controls repeatedly."
echo "Press Esc or close the window to stop."
echo ""

"$WINE" "$ROOT_DIR/build/dreamseeker-webview2.exe" \
    "$LOADER_WIN" "$USER_DATA_WIN" "$BASE_URL"
