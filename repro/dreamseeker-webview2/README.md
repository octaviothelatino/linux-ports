# DreamSeeker/WebView2 hover-flicker reproducer

This is one deliberately narrow reproducer for the SOA symptom. It is not a
visual copy of SOA and it does not contain a login flow. It reproduces the
native interface topology, not the flicker itself.

The host intentionally matches the rendering contract that matters:

1. A decorated 32-bit Win32 frame contains a visible `WS_CHILD` game client
   with the same clipping style observed on DreamSeeker (`0x56010000`).
2. A separate child of that game client owns a real Direct3D 9 device and
   presents an animated map continuously.
3. The host creates one short-lived advertisement WebView2 controller in a
   separate `BYOND: Your Game Is Starting` window, then four real WebView2
   controllers for the game. The ad window is hidden after navigation, but its
   controller and renderer remain alive. This matches the extra renderer and
   ad-to-game lifetime transition observed in DreamSeeker. The four game
   controllers match the observed SOA document surfaces:
   `a0284aed.htm` at 1366x696, `e33cfbfa.htm` at 1258x696, `b4175bf4.htm` at
   1x1, and `bdc319d8.htm` at 24x24.
   The host keeps the D3D9 map child below the asynchronously-created WebView2
   container children, so the map cannot hide or intercept them by accident.
   Controllers are initially created with the Win32 desktop window as their
   parent and then reparented into the game with `put_ParentWindow`, matching
   the desktop-to-game transition observed in DreamSeeker.
4. The documents are served by a local HTTP server and navigated as
   `http://127.0.0.1:<port>/<hash>.htm`, matching the URLs seen in the real
   DreamSeeker X11 tree. They are not loaded with `file://`.
5. Each document has a transparent body and ordinary static/interactive HTML.
   Hover changes only normal CSS control state; there is no timer, DOM removal,
   fake flicker, or code that deliberately hides pixels.
6. The host logs the Win32 hierarchy, process IDs, styles, bounds, controller
   parents, navigation results, and every WebView2 HRESULT needed to compare
   the test with DreamSeeker traces. The full hierarchy is printed at startup
   only; after navigation, the timer reports only changes to browser-owned
   `Chrome_WidgetWin_1` windows as `[html-state]` lines. It also reports
   changes to the native window under the real mouse cursor as `[hit-test]`
   lines. It does not continuously dump the whole tree or recursively walk
   the reparented hierarchy during the live test.

The window title identifies the animated D3D9 layer as the stable map. The
full-size HTML document is labeled `HTML A`, and the centered 1258x696 HTML
document is labeled `HTML B` and is the manual hover target. The 1x1 and 24x24
documents are labeled in the terminal/tree log because their exact sizes are
too small for readable on-screen subtitles. `HTML A` and `HTML B` also update
their native window titles with a hover-event count; this is instrumentation,
not a visual effect. If the count does not increase, the map or another native
window is receiving the mouse instead of WebView2. The `[hit-test]` line tells
us which one.

The previous single-controller/file-URL program was a false negative. It
proved only that one ordinary WebView2 page can sit above D3D9. It did not
exercise SOA's multiple overlapping cross-process document surfaces. This
version corrects that interface mismatch.

## Build and run

The WebView2 SDK header is intentionally not copied into this project. The
default expected location is `/tmp/webview2-sdk/WebView2.h`; override it with
`WEBVIEW2_SDK_HEADER=/path/to/WebView2.h` if needed.

Run with the isolated custom Wine build and test prefix:

```bash
cd /home/octavio/Downloads/linux-ports
./repro/dreamseeker-webview2/run.sh
```

The launcher has a conservative fallback command line for older baseline
runs. To exercise WebView2's normal D3D11/DirectComposition path, explicitly
provide an empty argument value:

```bash
BYOND_WEBVIEW_ARGS='' ./repro/dreamseeker-webview2/run.sh
```

An empty value is meaningful here; it is not the same as leaving the variable
unset. Leaving it unset restores the fallback flags, including
`--disable-d3d11`.

The paths can be overridden without editing the reproducer:

```bash
BYOND_PREFIX=/path/to/win32-prefix \
BYOND_RUNNER=/path/to/wine-build/install \
./repro/dreamseeker-webview2/run.sh
```

The run must be judged by the final desktop window, not merely by the DOM:

- **Pass/negative control:** the map stays visible, the HTML panel is fully
  visible, and hover transitions remain stable.
- **Reproducer match:** the map is healthy, but the HTML panel is absent or
  only portions appear/disappear when the pointer crosses the controls. The
   terminal must still report successful WebView2 environment, all five
   controller callbacks, and all five navigation callbacks.

The ad-transition multi-surface run is the new baseline. Do not change Wine
while evaluating it. If it does not flicker, the interface model still differs
from DreamSeeker and we must compare the logged hierarchy, URL type, bounds,
surface lifetime, and renderer count again before considering a Wine change.
