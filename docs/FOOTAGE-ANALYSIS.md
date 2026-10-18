# Footage analysis

Manual visual verification is the final authority for this problem. A screen
recording is useful supporting evidence, but it must not be used by itself to
declare the embedded panel fixed. Wine logs can show that WebView2 created a
controller or that Wine copied pixels internally, but those facts do not show
that the pixels reached the screen in the correct stacking order.

## Create one fresh recording

For the X11 test path, run:

```bash
./tools/byond-footage-test.sh byond://198.166.109.89:3336
```

The script clears only the marked `test-results/footage-latest` directory,
launches DreamSeeker for manual testing, and keeps one latest video. It then
creates optional diagnostic measurements under `analysis/`.

The default recording limit is 180 seconds. This covers the advertisement
transition and a manual check afterward. Set `BYOND_RECORD_DURATION` to a
larger number when necessary, or set it to `0` only when an unlimited session
is intentional.

On a Wayland desktop this captures the visible Wine X11 game window through
Xwayland, not the final native Wayland desktop composition. That limitation is
one reason the owner must perform the final visual check manually.

Use the analyzer in `tools/analyze-footage.py` only when diagnostic frame data
is useful:

```bash
python3 tools/analyze-footage.py \
  /absolute/path/to/recording.mp4 \
  --sample-fps 60 \
  --preview-fps 2 \
  --panel 500,80,360,360 \
  --game 50,28,1208,696
```

The default output is `test-results/footage-latest`. It is cleared at the
start of every run, so it contains only the newest analysis. The analyzer
refuses to clear a directory that it did not create. Use `--output` only when
you deliberately want another empty, analyzer-managed directory.

Use `--sample-fps 60` for recordings made at 60 frames per second. The
analyzer keeps the first forty changed frames for its contact sheet and saves
all changed frames individually, so long recordings do not exhaust memory.

The panel and game rectangles are coordinates in the recording. Adjust them
when the window position or display size changes. The output contains:

- `preview-contact-sheet.png`, a time ordered view of the recording.
- `change-contact-sheet.png`, frames with unusually large adjacent changes.
- `selected/`, the individual frames used for that contact sheet.
- `.footage-analysis-managed`, a safety marker used before clearing old output.
- `measurements.json`, numerical full frame and region changes for every sample.
- `report.txt`, a short summary of the largest changes.

Interpret the result together with the recording:

- Large full frame and game changes indicate whole game composition changes.
- Large panel changes with a stable game region indicate panel-only changes.
- A stable map with no panel change does not prove that the panel is visible.
  It can mean that the panel is underneath the map, so the X11 window tree and
  the actual desktop image must agree before calling the run a pass.

The analyzer does not manufacture flicker, change the video, or treat WebView2
navigation success as visual success. It also cannot reliably identify which
of SOA's separate windows supplied a visible login form. Its output is never a
substitute for the owner's manual pass or fail decision.

The original MP4 or WebM recording is never removed. Only generated analysis
files in the managed output directory are replaced.
