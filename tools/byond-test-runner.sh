#!/usr/bin/env bash

# Reproducible BYOND/WebView2 test runner.
# It launches one target, captures the X11 desktop/window tree at a fixed
# cadence, records the Wine process tree, and copies BYOND/WebView2 logs.

set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname -- "$SCRIPT_DIR")"

PREFIX="${BYOND_PREFIX:-/home/octavio/Games/byond32}"
RUNNER="${BYOND_RUNNER:-/home/octavio/.local/share/lutris/runners/wine/wine-11.14-staging-x86_64}"
MODE="hub"
GAME_URL="byond://198.166.109.89:3336"
DURATION=""
INTERVAL=""
KEEP_RUNNING=0
AUTO_CONTINUE=1
APP_EXITED=0
VIRTUAL_DESKTOP=0
INTERACTIVE=0

usage() {
    cat >&2 <<'EOF'
Usage:
  ./tools/byond-test-runner.sh --hub [--duration SECONDS] [--interval SECONDS]
  ./tools/byond-test-runner.sh --game URL [--duration SECONDS] [--interval SECONDS]
    [--keep-running]
  ./tools/byond-test-runner.sh --game URL --interactive

Environment overrides:
  BYOND_PREFIX             Wine prefix path
  BYOND_RUNNER             Lutris Wine runner path
  BYOND_TEST_RESULTS       Evidence directory (default: ./test-results)
  BYOND_WEBVIEW_ARGS       Complete WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS value
  BYOND_WEBVIEW_PROFILE    Windows path for WEBVIEW2_USER_DATA_FOLDER
  BYOND_WINEDEBUG          Wine debug channels
  BYOND_AUTO_CONTINUE      Set to 0 to disable automatic ad Continue click
  BYOND_SKIP_ADS           Set to 1 to dispatch the real pager://ok action
                           through WebView2 CDP instead of waiting for the ad
  BYOND_CDP_PORT            Local WebView2 DevTools port for BYOND_SKIP_ADS
                           (default: 9230)
  BYOND_VIRTUAL_DESKTOP    Set to 1 to launch inside Wine's 1366x724 desktop
  BYOND_CAPTURE_DESKTOP    Set to 1 to try an X11 root screenshot (usually
                           unavailable under Wayland/Xwayland; default: 0)
  BYOND_CAPTURE_IMAGES     Set to 0 for low-overhead telemetry-only runs;
                           window trees and process samples are still saved
  BYOND_CAPTURE_INVENTORY  Set to 0 to skip per-WebView xwininfo/xprop calls;
                           useful for clean long-run load/crash tests
  BYOND_CAPTURE_RECURSIVE  Set to 1 to capture the selected game window and
                           its X11 child windows recursively (default: 0)
  BYOND_WINDOW_PROBE       Optional Win32 probe executable to run at each
                           capture (for example ./win32-window-probe.exe)
  --keep-running            Leave the launched application alive when the
                           capture duration ends
  --interactive             Launch for manual play with no capture loop; the
                           runner waits until the application exits
  WINE_KEEP_CHILD_WINDOWS  Diagnostic-only custom-Wine experiment: preserve
                           a native X11 window during Win32 child reparenting

The default game run lasts 95 seconds with one screenshot per second so the
40-50 second advertisement transition and the following HTML form are both
captured. The prefix is stopped at the end unless --keep-running is supplied.
EOF
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --hub)
            MODE="hub"
            shift
            ;;
        --game)
            MODE="game"
            GAME_URL="${2:-}"
            [[ -n "$GAME_URL" ]] || usage
            shift 2
            ;;
        --duration)
            DURATION="${2:-}"
            shift 2
            ;;
        --interval)
            INTERVAL="${2:-}"
            shift 2
            ;;
        --keep-running)
            KEEP_RUNNING=1
            shift
            ;;
        --interactive)
            INTERACTIVE=1
            shift
            ;;
        --help|-h)
            usage
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            ;;
    esac
done

if [[ "$MODE" == "hub" ]]; then
    DURATION="${DURATION:-30}"
    INTERVAL="${INTERVAL:-3}"
    TARGET="$PREFIX/drive_c/Program Files/BYOND/bin/byond.exe"
    LABEL="hub"
else
    DURATION="${DURATION:-95}"
    INTERVAL="${INTERVAL:-1}"
    TARGET="$PREFIX/drive_c/Program Files/BYOND/bin/dreamseeker.exe"
    LABEL="game"
fi

if [[ "$INTERACTIVE" != 0 ]]; then
    DURATION="interactive"
fi

WINE_BIN="$RUNNER/bin/wine"
WINE_SERVER="$RUNNER/bin/wineserver"

if [[ ! -x "$WINE_BIN" ]]; then
    echo "Wine runner not found: $WINE_BIN" >&2
    exit 1
fi
if [[ ! -x "$TARGET" ]]; then
    echo "BYOND executable not found: $TARGET" >&2
    exit 1
fi

RESULTS_ROOT="${BYOND_TEST_RESULTS:-$PROJECT_ROOT/test-results}"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
OUT="$RESULTS_ROOT/${RUN_ID}-$LABEL"
mkdir -p "$OUT/screens"

exec > >(tee "$OUT/runner.log") 2>&1

DISPLAY_VALUE="${DISPLAY:-:0}"
PROFILE_WIN="${BYOND_WEBVIEW_PROFILE:-C:\\users\\octavio\\Documents\\BYOND\\cache\\WebView2\\WineTest\\${RUN_ID}-${LABEL}}"
# Keep the default aligned with the persistent Lutris configuration.  Older
# Wine builds sometimes needed --no-sandbox, but it must be opt-in for a
# controlled comparison because it changes the WebView2 process model.
# Leave BYOND_WEBVIEW_ARGS explicitly empty when testing the browser's native
# graphics path.  The default remains the known-good software fallback, but
# `${BYOND_WEBVIEW_ARGS-...}` lets the test matrix distinguish “unset” from
# “intentionally no extra flags”.
WEBVIEW_ARGS="${BYOND_WEBVIEW_ARGS---disable-gpu --disable-gpu-sandbox --disable-gpu-compositing --disable-d3d11 --disable-dwm-composition --use-angle=d3d9 --disable-background-networking --disable-metrics --disable-metrics-repo --disable-crash-reporter --disable-component-update --disable-features=msEdgeMetricsReporting,msWebOOUI}"
WEBVIEW_LOG_WIN="C:\\webview-wine-test-${RUN_ID}.log"
WEBVIEW_ARGS="$WEBVIEW_ARGS --enable-logging --v=1 --log-file=$WEBVIEW_LOG_WIN"
WINE_DEBUG="${BYOND_WINEDEBUG:-err+all,warn+dcomp,warn+d3d,warn+win,warn+x11drv}"
AUTO_CONTINUE="${BYOND_AUTO_CONTINUE:-$AUTO_CONTINUE}"
SKIP_ADS="${BYOND_SKIP_ADS:-0}"
CDP_PORT="${BYOND_CDP_PORT:-9230}"
if [[ "$SKIP_ADS" != 0 ]]; then
    if [[ "$MODE" != "game" ]]; then
        echo "BYOND_SKIP_ADS=1 is only supported with --game" >&2
        exit 2
    fi
    WEBVIEW_ARGS="$WEBVIEW_ARGS --remote-debugging-port=$CDP_PORT"
    AUTO_CONTINUE=0
fi
VIRTUAL_DESKTOP="${BYOND_VIRTUAL_DESKTOP:-$VIRTUAL_DESKTOP}"
CAPTURE_IMAGES="${BYOND_CAPTURE_IMAGES:-1}"
CAPTURE_INVENTORY="${BYOND_CAPTURE_INVENTORY:-1}"
DEBUG_FILE="$PREFIX/drive_c/users/octavio/Documents/BYOND/cfg/webview-debug.txt"
WEBVIEW_LOG_FILE="$PREFIX/drive_c/webview-wine-test-${RUN_ID}.log"

export WINEPREFIX="$PREFIX"
export WINEARCH=win32

echo "run_id=$RUN_ID"
echo "mode=$MODE"
echo "duration=$DURATION"
echo "interval=$INTERVAL"
echo "prefix=$PREFIX"
echo "runner=$RUNNER"
echo "display=$DISPLAY_VALUE"
echo "target=$TARGET"
echo "game_url=$GAME_URL"
echo "webview_profile=$PROFILE_WIN"
echo "webview_args=$WEBVIEW_ARGS"
echo "webview_log=$WEBVIEW_LOG_WIN"
echo "wine_debug=$WINE_DEBUG"
echo "auto_continue=$AUTO_CONTINUE"
echo "skip_ads=$SKIP_ADS"
echo "cdp_port=$CDP_PORT"
echo "virtual_desktop=$VIRTUAL_DESKTOP"
echo "capture_images=$CAPTURE_IMAGES"
echo "capture_inventory=$CAPTURE_INVENTORY"
echo "interactive=$INTERACTIVE"
"$WINE_BIN" --version

{
    echo '--- registry text ---'
    grep -n -A4 -B1 -E '^\[Software\\\\Wine(\\\\AppDefaults\\\\(msedgewebview2|dreamseeker)\.exe)?\]' "$PREFIX/user.reg" 2>/dev/null || true
    echo '--- WebView2 version ---'
    cat "$PREFIX/drive_c/users/octavio/Documents/BYOND/cache/WebView2/EBWebView/Last Version" 2>/dev/null || true
    echo
    echo '--- process snapshot before launch ---'
    ps -eo pid,ppid,stat,comm,args | grep -E 'wine|msedge|byond|dreamseeker' | grep -v grep || true
    echo '--- Lutris source checksum/status ---'
    git status --short 2>/dev/null || true
} > "$OUT/manifest.txt"

# BYOND appends to this file, so preserve the previous contents and start the
# run with an empty file. Otherwise a controller error from an older launch can
# be mistaken for a failure in the current run.
if [[ -f "$DEBUG_FILE" ]]; then
    cp -f "$DEBUG_FILE" "$OUT/webview-debug-before.txt"
    : > "$DEBUG_FILE"
fi

copy_log() {
    local src="$1"
    local dst="$2"
    if [[ -f "$src" ]]; then
        cp -f "$src" "$OUT/$dst"
    fi
}

capture() {
    local seq="$1"
    local now
    local tree_file
    local active_id
    local target_id
    local target_name
    local webview_id
    local embedded_id
    local compositor_id
    now="$(date +%s)"
    echo "capture=$seq epoch=$now"

    tree_file="$OUT/window-tree-$(printf '%04d' "$seq").txt"
    xwininfo -root -tree > "$tree_file" 2>&1 || true

    # Save a per-sample inventory for every WebView X11 window. This remains
    # available during telemetry-only runs, where ImageMagick is disabled so
    # the test itself does not add rendering load. It can be disabled for a
    # genuinely low-overhead long-run test: xwininfo/xprop for ten browser
    # processes is useful for lifecycle diagnosis but can itself add load.
    inventory_file="$OUT/webview-inventory-$(printf '%04d' "$seq").txt"
    if [[ "$CAPTURE_INVENTORY" != 0 ]]; then
        {
            echo "capture=$seq"
            while IFS= read -r id; do
                [[ "$id" == 0x* ]] || continue
                echo "window=$id"
                xwininfo -id "$id" 2>&1 |
                    grep -E 'Absolute upper-left|Relative upper-left|Width|Height|Map State|Parent window' || true
                xprop -id "$id" WM_NAME WM_CLASS _NET_WM_STATE _NET_WM_WINDOW_TYPE 2>&1 || true
            done < <(grep '("msedgewebview2.exe" "msedgewebview2.exe")' "$tree_file" |
                     awk '{print $1}' | sort -u)
        } > "$inventory_file"
    else
        : > "$inventory_file"
    fi

    if [[ "$CAPTURE_IMAGES" != 0 ]] && command -v import >/dev/null 2>&1; then
        target_id=""
        target_name=""
        if [[ "$MODE" == "hub" ]]; then
            target_id="$(grep -m1 'BYOND.*byond.exe' "$tree_file" | awk '{print $1}')"
            target_name="hub"
        else
            # The full game surface is the stable evidence source. The active
            # X11 window can be the hidden ad, a WebView helper, or even a
            # different desktop application after focus changes.
            target_id="$(grep -m1 'Shinobi Online Adventure.*dreamseeker.exe.*1366x' "$tree_file" | awk '{print $1}')"
            target_name="game"
            if [[ -z "$target_id" ]]; then
                target_id="$(grep -m1 'BYOND: Your Game Is Starting.*dreamseeker.exe' "$tree_file" | awk '{print $1}')"
                target_name="ad"
            fi
        fi
        if [[ "$target_id" == 0x* ]]; then
            timeout 3s import -window "$target_id" "$OUT/screens/${target_name}-$(printf '%04d' "$seq").png" 2>> "$OUT/screenshot-errors.log" || true
        fi

        # Keep an active-window capture as well.  When Wine briefly hands
        # focus to the WebView, this is the only X11-level evidence of the
        # native composition path; the stable game/hub capture above remains
        # the surface-quality measurement.
        active_id="$(xprop -root _NET_ACTIVE_WINDOW 2>/dev/null | awk '{print $NF}')"
        if [[ "$active_id" == 0x* ]]; then
            timeout 3s import -window "$active_id" "$OUT/screens/active-$(printf '%04d' "$seq").png" 2>> "$OUT/screenshot-errors.log" || true
        fi

        # WebView2 commonly owns a separate top-level X11 window under Wine.
        # Capture its real surface independently of the DreamSeeker map.
        webview_id="$(grep -m1 '("msedgewebview2.exe" "msedgewebview2.exe").*[0-9][0-9][0-9]x[0-9][0-9][0-9]' "$tree_file" | awk '{print $1}')"
        if [[ "$webview_id" == 0x* ]]; then
            # Geometry alone is not enough: Wine can expose the expected
            # 1016x509 WebView window while X11 reports it as IsUnMapped.
            # Record that state so DOM/controller success is not mistaken for
            # a visible overlay.
            xwininfo -id "$webview_id" > "$OUT/webview-window-$(printf '%04d' "$seq").txt" 2>&1 || true
            xprop -id "$webview_id" > "$OUT/webview-properties-$(printf '%04d' "$seq").txt" 2>&1 || true
            timeout 3s import -window "$webview_id" "$OUT/screens/webview-$(printf '%04d' "$seq").png" 2>> "$OUT/screenshot-errors.log" || true
        fi

        # When the WebView is embedded, its real content window is a nested
        # X11 child rather than the hidden 1016x509 helper above.  Capture
        # the login document separately so a blank game composite can be
        # distinguished from a blank browser surface.
        embedded_id="$(grep -m1 '127\.0\.0\.1.*msedgewebview2\.exe.*[0-9][0-9][0-9]x[0-9][0-9][0-9]' "$tree_file" | awk '{print $1}')"
        if [[ "$embedded_id" == 0x* ]]; then
            xwininfo -id "$embedded_id" > "$OUT/embedded-window-$(printf '%04d' "$seq").txt" 2>&1 || true
            timeout 3s import -window "$embedded_id" "$OUT/screens/embedded-$(printf '%04d' "$seq").png" 2>> "$OUT/screenshot-errors.log" || true
        fi

        # A DComp target is represented by an unnamed child below the real
        # local SOA document window. Capturing the document alone can show the
        # map while hiding the replacement surface that is actually being
        # tested. Capture that child separately when the expected 1258x740
        # document and its one child are present.
        compositor_id="$(awk '/127\.0\.0\.1/ && /1258x740/ { getline; if ($0 ~ /1 child:/) { getline; print $1; exit } }' "$tree_file")"
        if [[ "$compositor_id" == 0x* ]]; then
            xwininfo -id "$compositor_id" > "$OUT/compositor-window-$(printf '%04d' "$seq").txt" 2>&1 || true
            timeout 3s import -window "$compositor_id" "$OUT/screens/compositor-$(printf '%04d' "$seq").png" 2>> "$OUT/screenshot-errors.log" || true
        fi

        # import -window root is unreliable on Xwayland. xwd reads the same
        # X11 root and preserves the actual cross-window composition.
        if [[ "${BYOND_CAPTURE_DESKTOP:-0}" != 0 ]]; then
            timeout 5s sh -c 'xwd -root -silent | convert xwd:- png:-' > "$OUT/screens/desktop-$(printf '%04d' "$seq").png" 2>> "$OUT/screenshot-errors.log" || true
        fi
    fi

    {
        echo "--- active window ---"
        xprop -root _NET_ACTIVE_WINDOW 2>/dev/null || true
        echo "--- client stacking ---"
        xprop -root _NET_CLIENT_LIST_STACKING 2>/dev/null || true
        echo "--- process tree ---"
        ps -eo pid,ppid,stat,pcpu,pmem,rss,etime,comm,args |
            grep -E 'wine|msedge|byond|dreamseeker' | grep -v grep || true
        echo "--- system load ---"
        uptime 2>/dev/null || true
        free -h 2>/dev/null || true
    } > "$OUT/processes-$(printf '%04d' "$seq").txt"

    copy_log "$PREFIX/drive_c/users/octavio/Documents/BYOND/cfg/webview-debug.txt" "webview-debug-latest.txt"
    copy_log "$WEBVIEW_LOG_FILE" "webview-wine-test-latest.log"

    if [[ -n "${BYOND_WINDOW_PROBE:-}" && -x "$BYOND_WINDOW_PROBE" ]]; then
        timeout 3s env WINEDEBUG=-all "$WINE_BIN" "$BYOND_WINDOW_PROBE" \
            > "$OUT/window-probe-$(printf '%04d' "$seq").txt" 2>&1 || true
    fi
}

cleanup() {
    if [[ -n "${SKIP_ADS_PID:-}" ]] && kill -0 "$SKIP_ADS_PID" 2>/dev/null; then
        kill "$SKIP_ADS_PID" 2>/dev/null || true
    fi
    if [[ -n "${CLICK_PID:-}" ]] && kill -0 "$CLICK_PID" 2>/dev/null; then
        kill "$CLICK_PID" 2>/dev/null || true
    fi
    if [[ "$KEEP_RUNNING" -eq 0 && -n "${APP_PID:-}" ]] && kill -0 "$APP_PID" 2>/dev/null; then
        kill -TERM "$APP_PID" 2>/dev/null || true
        sleep 2
        kill -KILL "$APP_PID" 2>/dev/null || true
    fi
    if [[ "$KEEP_RUNNING" -eq 0 ]]; then
        "$WINE_SERVER" -k 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

has_game_webview() {
    local tree="$1"

    printf '%s\n' "$tree" | grep -q 'Shinobi Online Adventure.*dreamseeker.exe.*1366x' \
        && printf '%s\n' "$tree" | grep -Eq '127\.0\.0\.1:[0-9]+/[^" ]+\.htm'
}

click_continue_loop() {
    local start now elapsed line tree ad_id x y
    start="$(date +%s)"
    while true; do
        now="$(date +%s)"
        elapsed=$((now - start))
        (( elapsed >= 50 )) || { sleep 2; continue; }
        (( elapsed <= 120 )) || {
            echo "auto_continue_timeout elapsed=$elapsed"
            return 1
        }

        tree="$(xwininfo -root -tree 2>/dev/null)"
        line="$(printf '%s\n' "$tree" | grep -m1 '"BYOND: Your Game Is Starting"')" || {
            if has_game_webview "$tree"; then
                echo "auto_continue_complete elapsed=$elapsed"
                return 0
            fi
            sleep 2
            continue
        }

        # The root-tree geometry contains both coordinates relative to the
        # parent and absolute coordinates.  The former are usually +0+0 for
        # a reparented Wine window, so use xwininfo for the actual screen
        # position instead of parsing the first pair from the tree line.
        ad_id="$(printf '%s\n' "$line" | awk '{print $1}')"
        ad_info="$(xwininfo -id "$ad_id" 2>/dev/null || true)"
        if ! printf '%s\n' "$ad_info" | grep -q 'Map State: IsViewable'; then
            if has_game_webview "$tree"; then
                echo "auto_continue_complete elapsed=$elapsed"
                return 0
            fi
            sleep 2
            continue
        fi
        x="$(xwininfo -id "$ad_id" 2>/dev/null | sed -n 's/^  Absolute upper-left X: *\([-0-9]*\)$/\1/p')"
        y="$(xwininfo -id "$ad_id" 2>/dev/null | sed -n 's/^  Absolute upper-left Y: *\([-0-9]*\)$/\1/p')"
        if [[ -n "$x" && -n "$y" ]]; then
            # Continue is near the horizontal center and ~505 px below the
            # top of the 700x550 advertisement window.
            python3 "$SCRIPT_DIR/x11-click.py" "$((x + 350))" "$((y + 505))" || true
            echo "auto_continue_click elapsed=$elapsed x=$((x + 350)) y=$((y + 505))"
            sleep 5
        fi
    done
}

ENV_ARGS=(
    env
    "DISPLAY=$DISPLAY_VALUE"
    "GDK_BACKEND=x11"
    "SDL_VIDEODRIVER=x11"
    "XDG_SESSION_TYPE=x11"
    "WINEDLLOVERRIDES=d3d11=builtin;dxgi=builtin;d3d9=builtin;d3d12=builtin"
    "WINEDEBUG=$WINE_DEBUG"
    "WEBVIEW2_USER_DATA_FOLDER=$PROFILE_WIN"
    "WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=$WEBVIEW_ARGS"
)

if [[ "$VIRTUAL_DESKTOP" != 0 ]]; then
    if [[ "$MODE" == "hub" ]]; then
        CMD=("$WINE_BIN" explorer.exe "/desktop=byond,1366x724" "$TARGET")
    else
        CMD=("$WINE_BIN" explorer.exe "/desktop=byond,1366x724" "$TARGET" "$GAME_URL")
    fi
elif [[ "$MODE" == "hub" ]]; then
    CMD=("$WINE_BIN" "$TARGET")
else
    CMD=("$WINE_BIN" "$TARGET" "$GAME_URL")
fi

echo "launching=${CMD[*]}"
"${ENV_ARGS[@]}" "${CMD[@]}" > "$OUT/wine-console.log" 2>&1 &
APP_PID=$!
echo "launcher_pid=$APP_PID"

if [[ "$MODE" == "game" && "$SKIP_ADS" != 0 ]]; then
    python3 "$SCRIPT_DIR/byond-cdp-skip-ads.py" \
        --port "$CDP_PORT" \
        > "$OUT/skip-ads.log" 2>&1 &
    SKIP_ADS_PID=$!
    echo "skip_ads_pid=$SKIP_ADS_PID"
elif [[ "$MODE" == "game" && "$AUTO_CONTINUE" != 0 ]]; then
    click_continue_loop &
    CLICK_PID=$!
fi

if [[ "$INTERACTIVE" != 0 ]]; then
    echo "interactive_wait=1"
    # Reap the Wine wrapper instead of polling kill -0.  A completed Wine
    # wrapper can remain a zombie until wait(), which would otherwise leave
    # interactive mode attached forever after DreamSeeker exits.
    wait "$APP_PID" || true
    APP_EXITED=1
else
    START="$(date +%s)"
    SEQ=0
    while true; do
        NOW="$(date +%s)"
        ELAPSED=$((NOW - START))
        if (( ELAPSED >= DURATION )); then
            break
        fi
        if ! kill -0 "$APP_PID" 2>/dev/null; then
            APP_EXITED=1
            echo "app_exited_before_duration elapsed=$ELAPSED"
            break
        fi
        capture "$SEQ"
        SEQ=$((SEQ + 1))
        sleep "$INTERVAL"
    done

    capture "$SEQ"
fi
echo "completed=1"
echo "app_exited=$APP_EXITED"
if [[ -x "$PWD/byond-test-analyze.sh" ]]; then
    "$PWD/byond-test-analyze.sh" "$OUT" | tee "$OUT/analysis.txt"
fi
