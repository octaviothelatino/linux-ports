#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="$ROOT_DIR/build"
HEADER=${WEBVIEW2_SDK_HEADER:-/tmp/webview2-sdk/WebView2.h}
CC=${MINGW32_CC:-i686-w64-mingw32-gcc}

if [[ ! -f "$HEADER" ]]; then
    echo "WebView2.h not found: $HEADER" >&2
    echo "Set WEBVIEW2_SDK_HEADER to an installed WebView2 SDK header." >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"

"$CC" \
    -std=c11 \
    -O2 -w \
    -DUNICODE -D_UNICODE \
    -I"$ROOT_DIR/compat" \
    -I"$(dirname "$HEADER")" \
    "$ROOT_DIR/repro.c" \
    -o "$BUILD_DIR/dreamseeker-webview2.exe" \
    -mconsole \
    -ld3d9 -lole32 -loleaut32 -lshlwapi -lshell32 -luser32 -lgdi32 -luuid

echo "built $BUILD_DIR/dreamseeker-webview2.exe"
