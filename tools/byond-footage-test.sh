#!/usr/bin/env bash

# Record one manual BYOND run and analyze the resulting video.
# On Wayland, capture the Wine X11 game window directly because the X11 root
# does not contain the final Wayland composition.

set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname -- "$SCRIPT_DIR")"
GAME_URL="byond://198.166.109.89:3336"
MODE="game"
DISPLAY_VALUE="${DISPLAY:-:0}"
OUTPUT_ROOT="${BYOND_FOOTAGE_OUTPUT:-$PROJECT_ROOT/test-results/footage-latest}"
RECORD_SIZE="${BYOND_RECORD_SIZE:-}"
RECORD_FPS="${BYOND_RECORD_FPS:-30}"
RECORD_DURATION="${BYOND_RECORD_DURATION:-180}"
CAPTURE_BACKEND="${BYOND_FOOTAGE_BACKEND:-auto}"
FFMPEG_PID=""
RUNNER_PID=""

usage() {
    cat >&2 <<'EOF'
Usage:
  ./tools/byond-footage-test.sh [byond://server]

Environment overrides:
  DISPLAY                X11 display to record
  BYOND_FOOTAGE_OUTPUT  managed output directory
  BYOND_RECORD_SIZE     capture size such as 1366x768
  BYOND_RECORD_FPS      capture rate, default 30
  BYOND_RECORD_DURATION maximum seconds, default 180. Set to 0 for no limit
  BYOND_FOOTAGE_BACKEND auto, x11-root, or x11-window. Auto uses x11-window
                         in a Wayland session and x11-root otherwise

The script launches DreamSeeker for manual interaction. Close DreamSeeker
when the visual check is complete. The latest recording is saved as
recording.mp4 and its analysis is saved below the same managed directory.
EOF
    exit 2
}

if [[ $# -eq 1 && ( "$1" == "--help" || "$1" == "-h" ) ]]; then
    usage
fi
if [[ $# -gt 1 ]]; then
    usage
fi
if [[ $# -eq 1 ]]; then
    GAME_URL="$1"
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg is required" >&2
    exit 1
fi
if ! command -v xdpyinfo >/dev/null 2>&1; then
    echo "xdpyinfo is required" >&2
    exit 1
fi
if ! command -v timeout >/dev/null 2>&1; then
    echo "timeout is required" >&2
    exit 1
fi
if [[ "$CAPTURE_BACKEND" == "auto" ]]; then
    if [[ "${XDG_SESSION_TYPE:-}" == "wayland" ]]; then
        CAPTURE_BACKEND="x11-window"
    else
        CAPTURE_BACKEND="x11-root"
    fi
fi
case "$CAPTURE_BACKEND" in
    x11-root|x11-window)
        ;;
    *)
        echo "BYOND_FOOTAGE_BACKEND must be auto, x11-root, or x11-window" >&2
        exit 1
        ;;
esac
if [[ "$CAPTURE_BACKEND" == "x11-window" ]]; then
    if ! command -v xwd >/dev/null 2>&1; then
        echo "xwd is required for x11-window capture" >&2
        exit 1
    fi
    if ! command -v xwininfo >/dev/null 2>&1; then
        echo "xwininfo is required for x11-window capture" >&2
        exit 1
    fi
    if [[ -z "${BYOND_RECORD_FPS+x}" ]]; then
        RECORD_FPS=3
    fi
fi
if ! [[ "$RECORD_DURATION" =~ ^[0-9]+$ ]]; then
    echo "BYOND_RECORD_DURATION must be a nonnegative integer" >&2
    exit 1
fi

OUTPUT_ROOT="$(realpath -m -- "$OUTPUT_ROOT")"
if [[ "$OUTPUT_ROOT" == "/" || "$OUTPUT_ROOT" == "$PROJECT_ROOT" || -z "$OUTPUT_ROOT" ]]; then
    echo "Refusing unsafe footage output: $OUTPUT_ROOT" >&2
    exit 1
fi

prepare_output() {
    local marker="$OUTPUT_ROOT/.byond-footage-managed"
    local old_marker="$OUTPUT_ROOT/.footage-analysis-managed"

    if [[ -e "$OUTPUT_ROOT" && ! -f "$marker" && ! -f "$old_marker" ]]; then
        if [[ -n "$(find "$OUTPUT_ROOT" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
            echo "Refusing to clear an unmanaged directory: $OUTPUT_ROOT" >&2
            exit 1
        fi
    fi

    mkdir -p -- "$OUTPUT_ROOT"
    find "$OUTPUT_ROOT" -mindepth 1 -maxdepth 1 \
        ! -name .byond-footage-managed \
        ! -name .footage-analysis-managed \
        -exec rm -rf -- {} +
    printf '%s\n' 'This directory is managed by byond-footage-test.sh.' > "$marker"
}

stop_recording() {
    if [[ -n "$FFMPEG_PID" ]] && kill -0 "$FFMPEG_PID" 2>/dev/null; then
        kill -INT "$FFMPEG_PID" 2>/dev/null || true
        wait "$FFMPEG_PID" 2>/dev/null || true
    fi
}

on_signal() {
    stop_recording
    if [[ -n "$RUNNER_PID" ]] && kill -0 "$RUNNER_PID" 2>/dev/null; then
        kill -INT "$RUNNER_PID" 2>/dev/null || true
    fi
    exit 130
}

trap stop_recording EXIT
trap on_signal INT TERM

prepare_output

if [[ -z "$RECORD_SIZE" ]]; then
    RECORD_SIZE="$(xdpyinfo -display "$DISPLAY_VALUE" 2>/dev/null |
        sed -n 's/^[[:space:]]*dimensions:[[:space:]]*\([^[:space:]]*\).*/\1/p' |
        head -n 1)"
fi
RECORD_SIZE="${RECORD_SIZE:-1366x768}"

VIDEO="$OUTPUT_ROOT/recording.mp4"
FFMPEG_LOG="$OUTPUT_ROOT/ffmpeg.log"
RUNNER_RESULTS="$OUTPUT_ROOT/runner-results"

{
    echo "display=$DISPLAY_VALUE"
    echo "record_size=$RECORD_SIZE"
    echo "record_fps=$RECORD_FPS"
    echo "record_duration=$RECORD_DURATION"
    echo "capture_backend=$CAPTURE_BACKEND"
    echo "game_url=$GAME_URL"
    echo "video=$VIDEO"
    echo "runner_results=$RUNNER_RESULTS"
} | tee "$OUTPUT_ROOT/manifest.txt"

if [[ "$RECORD_DURATION" -gt 0 ]]; then
    timeout --foreground --signal=INT --kill-after=10s "${RECORD_DURATION}s" \
        env BYOND_TEST_RESULTS="$RUNNER_RESULTS" \
        BYOND_CAPTURE_IMAGES=0 \
        BYOND_CAPTURE_INVENTORY=0 \
        "$SCRIPT_DIR/byond-test-runner.sh" --game "$GAME_URL" --interactive &
else
    BYOND_TEST_RESULTS="$RUNNER_RESULTS" \
    BYOND_CAPTURE_IMAGES=0 \
    BYOND_CAPTURE_INVENTORY=0 \
    "$SCRIPT_DIR/byond-test-runner.sh" --game "$GAME_URL" --interactive &
fi
RUNNER_PID=$!

find_main_window() {
    local tree ids id info width height area
    local best_id=""
    local best_area=0
    tree="$(xwininfo -root -tree 2>/dev/null || true)"
    if [[ "$MODE" == "hub" ]]; then
        ids="$(printf '%s\n' "$tree" |
            grep '("byond.exe" "byond.exe")' | awk '{print $1}')"
    else
        ids="$(printf '%s\n' "$tree" |
            grep 'Shinobi Online Adventure' |
            grep '("dreamseeker.exe" "dreamseeker.exe")' | awk '{print $1}')"
    fi
    while IFS= read -r id; do
        [[ "$id" == 0x* ]] || continue
        info="$(xwininfo -id "$id" 2>/dev/null || true)"
        printf '%s\n' "$info" | grep -q 'Map State: IsViewable' || continue
        width="$(printf '%s\n' "$info" | sed -n 's/^  Width: *\([0-9]*\)$/\1/p')"
        height="$(printf '%s\n' "$info" | sed -n 's/^  Height: *\([0-9]*\)$/\1/p')"
        [[ "$width" =~ ^[0-9]+$ && "$height" =~ ^[0-9]+$ ]] || continue
        if [[ "$MODE" == "game" && ( "$width" -lt 1000 || "$height" -lt 600 ) ]]; then
            continue
        fi
        area=$((width * height))
        if (( area > best_area )); then
            best_area=$area
            best_id="$id"
        fi
    done <<< "$ids"
    printf '%s\n' "$best_id"
}

runner_is_alive() {
    [[ -n "$RUNNER_PID" ]] || return 1
    kill -0 "$RUNNER_PID" 2>/dev/null || return 1
    local state
    state="$(ps -p "$RUNNER_PID" -o stat= 2>/dev/null || true)"
    [[ "$state" != Z* ]]
}

capture_window_stream() {
    local window_id=""
    local previous_window_id=""
    while runner_is_alive; do
        window_id="$(find_main_window)"
        if [[ "$window_id" == 0x* ]]; then
            if [[ "$window_id" != "$previous_window_id" ]]; then
                echo "window_capture_id=$window_id" >> "$FFMPEG_LOG"
                previous_window_id="$window_id"
            fi
            timeout 2s xwd -id "$window_id" -silent 2>> "$FFMPEG_LOG" || true
        else
            sleep 0.05
        fi
    done
}

if [[ "$CAPTURE_BACKEND" == "x11-root" ]]; then
    ffmpeg -hide_banner -loglevel warning \
        -f x11grab -draw_mouse 1 -framerate "$RECORD_FPS" \
        -video_size "$RECORD_SIZE" -i "$DISPLAY_VALUE+0,0" \
        -c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p \
        -movflags +faststart "$VIDEO" > "$FFMPEG_LOG" 2>&1 &
    FFMPEG_PID=$!
else
    capture_window_stream |
        ffmpeg -hide_banner -loglevel warning \
            -f xwd_pipe -framerate "$RECORD_FPS" -i - \
            -c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p \
            -movflags +faststart "$VIDEO" > "$FFMPEG_LOG" 2>&1 &
    FFMPEG_PID=$!
fi

sleep 2
if ! kill -0 "$FFMPEG_PID" 2>/dev/null; then
    echo "ffmpeg could not start. See $FFMPEG_LOG" >&2
    exit 1
fi

echo "recording_started=$VIDEO"
echo "capture_backend=$CAPTURE_BACKEND"
echo "Close DreamSeeker after checking the panel and its flicker."
if [[ "$RECORD_DURATION" -gt 0 ]]; then
    echo "recording_maximum_seconds=$RECORD_DURATION"
fi

wait "$RUNNER_PID"
RUNNER_STATUS=$?
RUNNER_PID=""

if [[ "$RECORD_DURATION" -gt 0 && "$RUNNER_STATUS" -eq 124 ]]; then
    echo "recording_duration_reached=$RECORD_DURATION"
    RUNNER_STATUS=0
fi

stop_recording
FFMPEG_PID=""

ANALYSIS_DIR="$OUTPUT_ROOT/analysis"
python3 "$SCRIPT_DIR/analyze-footage.py" "$VIDEO" \
    --output "$ANALYSIS_DIR" \
    --sample-fps "$RECORD_FPS" \
    --preview-fps 2 || echo "analysis_failed=1"

echo "recording_complete=$VIDEO"
echo "analysis_directory=$ANALYSIS_DIR"
exit "$RUNNER_STATUS"
