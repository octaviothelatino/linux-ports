# BYOND native browser-control reproducer

This is the authoritative fast reproducer for the SOA-style interface
topology. It uses BYOND 516's own `BROWSER` skin controls over a native BYOND
`MAP` control, following the browser lifecycle used by Ter13's HudLib Widget
Pack Basic `TextInput` component. It does not call WebView2 directly and it
does not contain code that fakes or requests flicker.

## What the test contains

- The native map is a grid of maptext tiles with a moving `STABLE BYOND MAP`
  marker. It must remain visible around the HTML panel.
- The `.dmm` explicitly supplies the stable map turfs and area, so a missing
  map eye or missing map contents cannot be mistaken for a Wine compositor
  failure.
- At runtime, `mapbrowser` is created as a hidden browser matching the map.
  `hudinput` is created beside the map at `1x1`, then moved, resized, shown,
  and focused over the map. This is the important HudLib-style lifecycle.
- The local HTML page has a centered panel and three CSS hover buttons. The
  HTML body is transparent, so the native map should be visible outside the
  browser rectangle.
- The HTML hover counter is only an observation aid. It counts genuine
  `mouseenter` events; it does not animate, hide, or redraw the panel.
- The `.dmf` contains only the native MAP. The browser controls are created by
  `winset()` at runtime, as they are in the HudLib text-input implementation.

## Build and run

The project can be edited with Linux tools. The default build uses the
installed Windows `dm.exe` under the isolated upstream Wine prefix, and the
default run uses Windows `dreamseeker.exe` from that same prefix:

```bash
cd /home/octavio/Downloads/linux-ports/repro/byond-browser-control
./build.sh
./run.sh
```

The launcher forces WineD3D for this comparison, matching the project
baseline: `d3d9`, `d3d11`, `dxgi`, and `d3d12` use Wine's builtin DLLs.
Override `WINEDLLOVERRIDES` only when deliberately testing a different
renderer.

Override the test prefix or runner without changing the project:

```bash
BYOND_PREFIX=/path/to/test-prefix \
BYOND_RUNNER=/path/to/wine-build/install \
./build.sh
```

Do not use the working `/home/octavio/Games/byond32` prefix for the first
reproducer run. The project must first be checked with the isolated baseline,
then with the proposed per-process Windows-version split, and only then with
the known-good prefix if a comparison is needed.

## Manual acceptance test

1. Confirm the window title identifies the native BYOND map test.
2. Before the HTML surface appears, confirm the map and the
   `HUDLIB-STYLE BROWSER` map label are visible. Click anywhere inside the
   native map to trigger the hidden `mapbrowser:onTextInput` JavaScript
   handoff. The test includes a native click fallback because maptext-only
   labels are not guaranteed to receive mouse input in every
   DreamSeeker/Wine combination.
   If the entire map is black, the mob does not have a map location and the
   run is invalid; the test now assigns one during `Login()`.
3. After the runtime browser appears, confirm the map remains visible outside
   the HTML rectangle. A full-window white surface is a reproducer setup
   failure, not a valid flicker result.
4. Move the pointer repeatedly across all three HTML buttons. Also use Show
   HTML HUD and Hide HTML HUD repeatedly to exercise the resize/show/focus
   lifecycle.
5. A normal pass has stable HTML, visible map around it, changing hover styles,
   and an increasing hover counter.
6. A failure is a stable map with HTML that is missing, partly drawn, erased,
   or visibly flickering when the pointer crosses the buttons or when the
   browser is shown again.

During diagnosis, the window title reports the browser lifecycle. `INIT` is
the off-screen `1x1` staging state; `CLIENT_CLICK_ACCEPTED` or `MOUSEDOWN`
means DreamSeeker received input; `HANDOFF_SENT` means DM sent the
`mapbrowser:onTextInput` event; `JS_HANDOFF_RECEIVED` and `JS_WINSET_SENT`
mean the hidden browser executed the HudLib-style JavaScript handoff. The
`OVERLAY_DOM_LOADED` marker means the actual dynamic `overlay.html` document
reached `DOMContentLoaded`; it does not prove that its pixels were presented.
following title state includes `winget()`'s actual `hudinput` rectangle. The
HTML panel is not a valid compositor test until that rectangle changes from
`pos=-1,-1;size=1x1` to approximately `size=400x330`.

This project is useful only if the failure comes from BYOND/Wine. It must not
be changed to draw a fake flicker. If it stays stable under both configurations,
it is a negative control and SOA still has a missing interface/lifetime detail.

## Configuration hypothesis

The proposed compatibility test is deliberately separate from this project's
rendering model:

| Scope | Proposed Wine version |
|---|---|
| Prefix default | Windows 7 |
| `byond.exe` | Windows 7 |
| `dreamseeker.exe` | Windows 10 |
| `msedgewebview2.exe` | Windows 10 |

The split is not applied by these scripts. It should be applied only to a
disposable test prefix after the baseline run. BYOND added
[`inner-background-color`](https://www.byond.com/docs/notes/516.html) browser
transparency in 516.1680; WebView2 documents that transparent
[`DefaultBackgroundColor`](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controller2?view=webview2-1.0.3650.58)
is rejected under Windows 7.

## Source references

- BYOND browser-control reference: the installed 516.1686 reference at
  `drive_c/Program Files/BYOND/help/ref/info.html`.
- [HudLib: Core by Ter13](https://www.byond.com/developer/Ter13/HudLibCore)
- [HudLib: Widget Pack (Basic) by Ter13](https://www.byond.com/developer/Ter13/HudLibWidgetPackBasic)
- [BYOND 516 release notes](https://www.byond.com/docs/notes/516.html)
- [BYOND browser transparency issue](https://www.byond.com/forum/post/2963117)
- [BYOND download page](https://www.byond.com/games/download/)
