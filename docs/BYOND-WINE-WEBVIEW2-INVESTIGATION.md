# BYOND 516 / Wine / WebView2 investigation

Status: active investigation; `origin/master` remains the clean upstream
baseline, while the separate `dcomp-composition` branch contains uncommitted
experimental source changes; no production-ready Wine fix exists

Last updated: 2026-09-11

## Manual visual authority correction - 2026-09-11

The owner's manual verification is the authority for whether the SOA interface
works. Automated footage and frame measurements are diagnostic evidence only.

The current symptom needs to be described precisely:

- The old per-control hover flicker is not happening in the current runs. It
  was already absent before the latest source changes, so no recent Wine
  change may be credited with fixing it.
- The remaining visible problem is flicker or intermittent disappearance of
  the entire embedded HTML panel.
- SOA also has a backup login form in a separate window. That form is not the
  embedded WebView2 surface under investigation and must not count as a pass.

This correction changes the test decision. A run passes only when the owner
manually confirms that the embedded panel stays visible over the map, remains
inside DreamSeeker, disappears cleanly after login, and does not become a
window above unrelated applications. A recording may document the run, but it
cannot distinguish the embedded form from the backup window reliably enough to
make that decision automatically.

## Manual Gate C retest and log correlation - 2026-09-11

### Expected behavior

The embedded login surface should stay continuously visible over the map. The
whole surface should remain stable while the pointer moves, and it should stay
inside DreamSeeker.

### Observed behavior

The owner manually confirmed all of the following:

1. The whole embedded HTML panel flickers.
2. The panel still flickers when the pointer is not causing the old
   per-control repaint symptom.
3. The panel disappears often enough that login is impossible.
4. The panel does not appear above unrelated applications.

The 5.4 second recording at
`/home/octavio/Videos/Screencasts/Screencast_20260911_102057.mp4` agrees with
that result. The complete form is visible in some frames and absent in others.
The recording does not show the individual controls repainting independently.

### Log correlation

The exact run is `test-results/20260911-101647-game`. Its ad-skip log shows
that the real advertisement action completed and that the game URL was
opened. The Wine log then records:

- six failures to obtain a shared texture handle, with `E_NOTIMPL`
  (`0x80004001`)
- 61 unsupported D3D11 texture interface queries returning `E_NOINTERFACE`
- repeated unimplemented `GetDeviceRemovedReason` calls
- one DirectComposition warning during shutdown stating that the composition
  parent was no longer a window

WebView2's own log independently reports repeated failures to obtain an
offscreen texture shared handle with `HRESULT: 0x80004001`. This corroborates
that the browser is producing a surface that Wine cannot consistently pass
through the shared-texture presentation path. It does not prove that every
visible transition is caused by one specific call.

### Test scope and result

This run forced `WINE_D3D_CONFIG=renderer=gl`. Therefore it is a valid failed
manual result for the OpenGL WineD3D path, but it is not a comparison against
the default renderer. The result is **Gate C failed**. The absence of the old
hover-only symptom must not be credited to this Wine change because it was
already absent before the current source work.

### Next discriminating test

Use one manual run with the same source and prefix, native WebView2 arguments,
and `WINE_D3D_CONFIG` unset. Compare only whether the complete embedded panel
stays visible. Do not add recording analysis or another source change to that
comparison.

## Renderer comparison result - 2026-09-11

The manual comparison with `WINE_D3D_CONFIG` unset produced the same four
answers as the OpenGL run. The entire embedded panel still flickered, login
was impossible, and the panel did not escape above unrelated applications.
The old per-control hover symptom remained absent.

The logs changed in detail but not in outcome:

| Signal | OpenGL run | Default renderer run |
| --- | ---: | ---: |
| `GetSharedHandle` failures | 6 | 0 |
| unsupported D3D11 texture interface queries | 61 | 95 |
| unimplemented device status queries | 177 | 228 |
| DirectComposition parent warning | 1 | 1 |

The OpenGL run reached the missing legacy shared-handle call directly. The
default-renderer run instead made more unsupported D3D11 interface queries.
Both paths still failed to present the complete HTML surface. This rejects
the hypothesis that choosing OpenGL or leaving the renderer at its default is
enough to fix SOA. It also moves the likely failure above the Linux graphics
backend choice, into the common D3D11 shared-resource and DirectComposition
presentation contract.

The DirectComposition warning in both runs occurred during shutdown, after the
application was closed. It is useful lifetime evidence, but it is not proof
that the warning caused the live panel flicker.

The renderer configuration is now a documented negative result. No further
renderer toggles should be tested before changing the relevant Wine graphics
implementation or adding a narrowly targeted trace for the common presentation
path.

## Latest focused Wine validation - 2026-09-04

This entry corrects an earlier reading of the X11 tree. The `xwininfo` tree
prints children from topmost to bottommost. The Xlib `XQueryTree` function uses
the opposite order. A small Xlib check verified this difference on the same
display.

### Expected behavior

Windows keeps the DreamSeeker map visible while the embedded WebView2 login
surface remains visible above it. An older browser surface must not cover the
current login surface.

### Observed behavior

The latest manual SOA run has the login surface and its nested browser child
viewable. Two live tree readings ten seconds apart had the same order. In the
topmost-first order printed by `xwininfo`, the relevant children were:

1. the 24 by 24 browser icon
2. the 1258 by 740 current login surface
3. the 17 by 17 browser icon
4. the 1 by 1 browser surface
5. the old 1366 by 740 browser surface

Therefore the current login surface is above the old surface. This is the
opposite of the earlier mistaken conclusion that the old surface was on top.

### Source change tested

The cumulative experimental change is in
`/home/octavio/Downloads/wine-upstream/dlls/winex11.drv/window.c`.

It now does three relevant things in the cross-process browser path:

- preserves the existing X11 window when the Win32 browser window becomes a
  child of DreamSeeker
- maps a preserved child with `XMapRaised` when it becomes normal and visible
- performs the reparent and its flush through the browser window's own X11
  display connection

The earlier changes that avoid an extra raise during ordinary show and map
processing remain in the experimental tree. Only the X11 driver was rebuilt
for this check. The working Lutris runner and working prefix were not changed.

### Result

At the X11 window-tree level, the latest build passes the stacking test. The
browser surface is present, the nested content child is viewable, and the
relative order stayed stable during the two readings.

This is not yet proof that the complete SOA visual behavior is fixed. The
automatic Wayland screen capture is unavailable in this session, and an X11
parent capture does not include all of its child windows. The same build is
left open for manual screen verification.

The manual run `20260904-191118-game` remained alive for more than thirteen
minutes. A direct capture of its nested browser child showed a complete SOA
login form, including the fields and buttons. The Wine log still contains
existing `dcomp:visual_SetContent` and `dcomp:device_Commit` semi-stub messages,
as well as `dxgi_surface_GetDC` semi-stub messages. Those messages show that
the broader DirectComposition and D3D11 path is not fully implemented. They do
not by themselves prove that the current stacking failure is caused by those
stubs.

The owner’s 10.17 second recording `Screencast_20260905_005033.mp4` is the
authoritative visual check. It shows the map remaining visible while the login
panel appears briefly, disappears, and appears again several times. The panel
is therefore not stable during the real composed run. The X11 tree result and
the nested browser capture were useful diagnostics, but they were not a full
pass. The current source change must be treated as an unsuccessful partial
change, not as a fix.

### Controlled driver comparison - 2026-09-05

The nearest available pre change binary was
`/tmp/winex11-before-xraise.so`. It contains the earlier child preservation
change but not the later `XMapRaised` change. The current installed binary was
saved as `/tmp/winex11-current-compare.so` before the comparison and restored
afterward.

Both runs used the same isolated prefix, Wine build, WebView2 arguments, ad
skip action, 20 second duration, one second capture interval, and one click at
screen coordinate 680,380. The pre change run was
`/tmp/soa-compare-baseline/20260905-010200-game`. The current run was
`/tmp/soa-compare-current/20260905-010433-game`.

The click did not open a visible login panel during either short run. The map
captures at sample 14 were byte identical. The final X11 tree in both runs had
the same five browser children under DreamSeeker, in the same topmost first
order: 24 by 24, 1258 by 740, 17 by 17, 1 by 1, and 1366 by 740. This test did
not reach the visual transition shown in the owner’s recording, so it is
inconclusive rather than evidence that the two drivers behave identically in
the failing case.

### Native D3D11 diagnostic run - 2026-09-05

#### Expected behavior

WebView2 should submit its rendered pixels through DirectComposition, and Wine
should publish those pixels in a visible child surface above the DreamSeeker
map. Moving the pointer over the HTML should not change whether the surface is
visible.

#### Observed behavior

The run used `BYOND_WEBVIEW_ARGS=''`, which removed the runner’s software
fallback flags. WebView2 therefore used its native D3D11 and DirectComposition
path. The owner reproduced the usual symptom: the HTML panel was rendered but
was not displayed continuously.

The trace proves all of the following:

- WebView2 associated 15 swap chains with DirectComposition visuals.
- DirectComposition started its composition thread and copied 256 frames from
  D3D11 surfaces successfully.
- The destination windows were visible during the recorded calls.
- No preserved browser child was unmapped, restored, or repeatedly reparented
  during the failure.
- Wine reported 1,105 failures from `UpdateLayeredWindow`, every one with
  error 87, which is `ERROR_INVALID_PARAMETER`.

The failure is therefore not currently explained by a missing D3D11 frame or
by a repeated X11 reparent operation. Wine reaches the final bridge that turns
the copied pixels into a visible layered window, and that bridge rejects many
updates.

The raw log grew to 3.1 GB because the X11 driver trace included a very noisy
polling path. After the relevant counts and timestamps were extracted, that
disposable raw file was removed to recover temporary disk space. The run
summary remains in `/tmp/soa-diagnostic-native-20260905/20260905-210825-game`.

### Renderer switch test: WebView2 OpenGL - 2026-09-06

#### Expected behavior

Changing WineD3D's Linux graphics backend from Vulkan to OpenGL should leave
the Windows-facing WebView2 Direct3D 11 interface intact. If Vulkan were the
cause of the presentation failure, the login surface should become stable in
this run.

#### Change tested

This was one isolated run using the same isolated prefix, custom Wine build,
native WebView2 arguments, built-in Wine D3D11 and DXGI, and DirectComposition
trace settings as the preceding diagnostic run. The only graphics change was
the temporary environment setting `WINE_D3D_CONFIG=renderer=gl`. It forced
WineD3D to use OpenGL for DreamSeeker and every WebView2 child process. The
working prefix was not changed.

#### Observed behavior

The owner saw the same HTML flicker. The log confirms that two WineD3D
processes selected OpenGL and none selected Vulkan. The browser still called
`CreateSwapChainForComposition`, which Wine logged as a stub. Wine also logged
`Unimplemented swap effect 0x3` and `dxgi_surface_GetDC` as a semi-stub.
The experimental DirectComposition bridge published 256 frames successfully,
but three surface creations still failed with error 87 during the run. The
previous run's repeated `UpdateLayeredWindow` failures did not occur in this
shorter test.

#### Result and interpretation

This rejects the narrow hypothesis that Vulkan alone causes the SOA flicker.
WebView2 continues to use Direct3D 11, but changing the Linux backend does not
repair the missing presentation. The stronger remaining hypothesis is that
Wine's composition swapchain contract is incomplete: the current
`CreateSwapChainForComposition` implementation creates an ordinary windowed
swapchain and then relies on the experimental bridge to copy its pixels into a
replacement window. That is not the same operation as handing a composition
swapchain to DirectComposition. This is a source-level hypothesis, not yet a
proven root cause.

#### Next discriminating test

Inspect and test the DXGI composition-swapchain path before changing the X11
parenting code again. The next source experiment should be limited to how
`CreateSwapChainForComposition` creates and exposes its swapchain, followed by
the same real DreamSeeker run and the existing DXGI tests. A stable surface
after that change would implicate the swapchain contract. An unchanged result
would move the investigation back to DirectComposition target presentation.

#### Current model

The smallest model consistent with the evidence is:

1. WebView2 creates and presents a D3D11 swap chain.
2. Wine’s experimental DirectComposition code copies that swap chain into a
   GDI-compatible bitmap.
3. Wine calls `UpdateLayeredWindow` for the replacement child surface.
4. Some of those calls are rejected with error 87, so the screen can retain an
   older frame, a blank frame, or no visible panel even though new HTML pixels
   exist.

This identifies the failing stage, but not yet the exact rejected parameter.
In `dlls/win32u/window.c`, error 87 can result from an invalid flag, a missing
`WS_EX_LAYERED` style, an already initialized layered-window attribute, or a
non-positive size. The DirectComposition trace shows positive surface sizes
and uses the valid `ULW_ALPHA` flag, but the run did not enable the `win32u`
warning that prints the target style. We must not call the exact style or
layered-state cause proven yet.

#### Next discriminating test

Run one more SOA session with the same native WebView2 settings, no X11 trace,
and `warn+win32u` enabled. The `NtUserUpdateLayeredWindow` warning will show
the target extended style at the moment error 87 occurs. If
`WS_EX_LAYERED` is missing, the fix belongs in the replacement surface style
or its style synchronization. If it is present, the next check is whether
Wine has marked the surface as already initialized by
`SetLayeredWindowAttributes`, which would make repeated
`UpdateLayeredWindow` calls invalid. This is the next test because it changes
logging only and distinguishes those two code paths directly.

### DXGI composition swapchain implementation test: 2026-09-06

#### Expected behavior

`CreateSwapChainForComposition` is supposed to create a swapchain for a
DirectComposition visual. It is not supposed to create an ordinary visible
window and then pretend that the window is the composition surface. The
official Wine proposal for this API uses no application window, keeps a
private one pixel helper window for internal WineD3D work, and marks the
swapchain as a composition swapchain.

#### Change tested

The source change was limited to the DXGI implementation. The factory now
validates the composition description and passes a null window to the D3D11
swapchain factory. The D3D11 swapchain records whether it is a normal HWND
swapchain or a composition swapchain. For the latter, it creates and destroys
only an internal one pixel helper window. Operations that require an HWND,
such as fullscreen changes and `GetHwnd`, now return the documented invalid
call result for composition swapchains.

This is adapted from Wine merge request 2245, which is still a draft. It does
not change the X11 driver, DirectComposition target code, or the OpenGL
selection. The working prefix remains unchanged.

#### Build and observed log behavior

The 32 bit DXGI module compiled successfully and was installed only into the
custom runner. The isolated SOA run is `20260906-231633-game` under
`/tmp/soa-dxgi-composition-20260906`.

Wine no longer logs the old `stub` message for
`CreateSwapChainForComposition`. It reaches the new composition path. The
same run still logs `Unimplemented swap effect 0x3`, repeated
`dxgi_surface_GetDC` semi-stub messages, and shared-handle warnings. The
DirectComposition bridge recorded 512 surface updates whose destination was
reported as not visible. No composition-surface creation or publication
failure was reported in this run.

#### Result and interpretation

The source experiment is active and its intended code path is running. The
log does not yet prove that the visible SOA bug is fixed. If the panel remains
hidden or flickers, the remaining failure is after swapchain creation, in the
way Wine exposes the composition surface to the DirectComposition target.
If the panel becomes stable, the old ordinary-window fallback was the cause.

#### Next discriminating test

Finish the current manual visual check before changing more code. Inspect
whether the HTML panel is continuously visible above the map while moving
the pointer across it. Do not change the prefix, renderer, or another Wine
subsystem during this check.

### Current model

The evidence supports a real stale-window ordering problem in the experimental
cross-process X11 preservation path. The latest change appears to correct the
order at the X11 level and allows the browser pixels to appear. The recording
shows that another step still removes them from the final composed frame, or
that the browser surface is repeatedly invalidated without being repainted.
The DirectComposition bridge is still experimental and is not ready to
describe as a final Wine fix.

### Next discriminating test

The next single test should compare one identical SOA interaction with the
current X11 change and with that change removed. The recording must begin
before the panel is opened and continue after pointer movement. Compare whether
the panel is absent, intermittently visible, or continuously visible. Do not
change the DirectComposition code during that comparison.

The project now has `tools/byond-footage-test.sh` for the X11 path. It records
one fresh FFmpeg video while DreamSeeker is used manually, clears only its
marked latest-footage directory, and runs `tools/analyze-footage.py` after the
run. The analyzer measures frame changes, but the recording itself remains the
final visual authority. This tool does not record a native Wayland desktop.

## Goal

Make BYOND 516 work reliably under Wine on Linux so that:

1. `byond.exe` (the BYOND Hub/Pager) does not open blank or black.
2. `dreamseeker.exe` reaches the game normally.
3. WebView2/TGUI HTML forms remain visible over the DreamSeeker map without flickering.
4. The eventual fix is generic Wine behavior that can help other WebView2 applications, rather than a BYOND-specific hack.

The game used for the DreamSeeker test is **Shinobi Online Adventure**. It is not our game and its source code is unavailable to us. The map and login HTML are known-good because the same screen renders correctly on Windows and the HTML appears intermittently under Wine.

## Environment

- OS: Debian GNU/Linux 13 (trixie), x86_64
- GPU: Intel UHD Graphics 620, `i915`
- Multiarch: enabled; 32-bit Wine libraries are installed
- BYOND: 516 build `516.1686`, 32-bit Windows application
- Working prefix: `/home/octavio/Games/byond32`
- Working Lutris runner: `/home/octavio/.local/share/lutris/runners/wine/wine-11.14-staging-x86_64`
- Lutris configuration files:
  - `/home/octavio/.local/share/lutris/games/byond32-1786423793.yml`
  - `/home/octavio/Downloads/linux-ports/config/lutris-byond32.yaml`

The working runner and prefix must remain unchanged while experimental runners are tested.

The clean upstream baseline is `origin/master` in the separate Wine checkout.
The active experiment is on the separate `dcomp-composition` branch, and its
changes are intentionally uncommitted while the behavior is being validated.
The working Lutris source and prefix are not used for this experiment.

## Standalone reproducer status — abandoned 2026-08-17

An instant standalone WebView2/D3D9 harness was attempted to shorten iteration
time. It created the form successfully, but never reproduced the real symptom:
the form did not fragment or flicker when the pointer hovered over controls.
Because it was not an exact reproduction, its source and generated results
were removed. The real DreamSeeker/SOA run remains the authoritative test.

The observed symptom remains: the map loads, the HTML login page is alive, and
hovering over the invisible form causes partial regions to appear and vanish.
No Wine change should be justified by the discarded harness.

### Next reproducer: a real BYOND browser control

The standalone C harness is not the right behavioral authority because it
creates WebView2 controllers directly and does not pass through DreamSeeker's
BYOND browser-control implementation. The next reproducer must be a tiny
BYOND world containing:

1. A continuously changing native map or map-like scene, representing the
   stable DreamSeeker/D3D9 layer.
2. Real BYOND browser controls positioned over that scene, including the
   runtime create/reparent/resize/show/focus lifecycle used by the target HUD
   library.
3. A local HTML page with a visible panel and ordinary CSS hover controls.
4. The BYOND 516 browser transparency setting (`inner-background-color`) so
   the page is transparent around the panel, like the SOA HUD.

The test must not contain artificial flicker. Wine must be allowed to produce
the result. We will first confirm that the map and browser panel are correct
under the known-good configuration, then run the identical world with the
upstream Wine build and manually test hover behavior.

This project can build that world on Debian. The official BYOND Linux package
includes the DreamMaker compiler but is a development/hosting platform rather
than a game-playing client. Therefore the source can be written with a normal
Linux editor and compiled with Linux DreamMaker (or compiled by Windows Dream
Maker under Wine); the resulting world is then launched by Windows
DreamSeeker under Wine. See:

- <https://www.byond.com/games/download/>
- <https://www.byond.com/forum/post/724526>

The first implementation task is to determine the exact 516.1686 skin/control
syntax for `inner-background-color`, then create this minimal world. Until it
exists, the C harness remains only a negative control and no compositor patch
should be evaluated against it.

### BYOND reproducer created — 2026-08-18

The real BYOND reproducer is now written at:

```text
/home/octavio/Downloads/linux-ports/repro/byond-browser-control/
```

It contains one `.dme` project, one DM source file, one `.dmf` skin, and one
packaged local HTML page. The DMF places a native `MAP` control below a real
`BROWSER` control whose `inner-background-color` is `transparent`. The map
contains visible maptext and a moving marker; the HTML contains a centered
panel and normal CSS hover buttons. No code intentionally creates flicker.

The project compiled successfully with the installed BYOND compiler:

```text
DM compiler version 516.1686
browser-control-repro.dmb - 0 errors, 0 warnings
```

The launcher was corrected to force the documented WineD3D baseline rather
than accidentally selecting DXVK. The first run is currently a manual
validation run; no pass/failure claim is recorded until the map, transparent
area, HTML panel, and hover behavior have been inspected on screen. The
Win7/Win10 split remains un-applied and is a separate later comparison.

The first visual launch exposed an implementation mistake in the reproducer:
using `browse(..., "window=htmlpanel")` created a separate browser popup with
its own title bar. That run was invalid for the embedded-interface test. The
project was corrected by marking the static browser control as the default
browser and calling `browse()` without a `window=` option. The corrected X11
window list contains only the main reproducer window, so the HTML is now
embedded in the BYOND skin rather than being a separate top-level popup.

The next visual check found a second project mistake: the `.dme` did not
include the `.dmf` skin at all, so DreamSeeker loaded BYOND's default skin.
That produced a side-by-side map/browser layout and was also invalid for the
overlay test. The project now includes `browser-control-repro.dmf` explicitly;
the next run must show the custom title and the MAP/BROWSER overlap defined by
that file.

### HudLib topology correction — 2026-08-18

The custom-skin run then showed a full-window white browser surface. That was
not a flicker result: the browser control owned the entire client rectangle,
so the native map was not visible underneath it. The correct question is not
whether a full-page browser can render; it is whether a browser surface that
is dynamically attached to the map's parent can render while the map remains
visible around it.

Ter13's packages were inspected from their public BYOND downloads:

- [HudLib: Core](https://www.byond.com/developer/Ter13/HudLibCore) constructs
  native `/hud` objects and puts them into `client.screen`; it does not create
  browser controls and is therefore not, by itself, a WebView2 reproducer.
- [HudLib: Widget Pack (Basic)](https://www.byond.com/developer/Ter13/HudLibWidgetPackBasic)
  contains the relevant browser-backed `TextInput` implementation. Its
  `InitializeInputs()` procedure creates a hidden full-size `mapbrowser`, then
  creates a `hudinput` browser at `1x1` and `-1,-1`. When a native HUD input is
  clicked, JavaScript moves and resizes `hudinput` over the map, shows it, and
  focuses it; blur hides it and returns it to `1x1`.

The reproducer has now been changed to this single HudLib-style lifecycle. Its
DMF contains only the native map. At runtime the DM code creates `mapbrowser`
and `hudinput`, loads the two local documents into those named browser
controls, and sends `mapbrowser:onTextInput` when the native trigger is
clicked. The hidden browser's JavaScript then moves and resizes `hudinput` to a
finite panel rectangle. The HTML hover events remain genuine and no flicker is
synthesized.

The next manual run is valid only if the native map is visible before and
around the finite HTML rectangle. If the entire client becomes white again,
that is evidence that Wine's browser surface is opaque or incorrectly
composited, but it is not yet the specific hover-flicker observation. The
`Show HTML HUD`, `Hide HTML HUD`, and `Reload HTML` verbs exercise the same
runtime visibility and geometry transitions as the library.

The first run of this dynamic version also exposed a test-world error: the
reproducer mob had no `loc`, so DreamSeeker's MAP control had no eye position
and appeared black. `Login()` now places the mob at `(20,14,1)`. A black map
must therefore be treated as an invalid reproducer run, not as evidence about
Wine's browser compositor.

The following check found that the project also had no explicit `.dmm` map.
The project now includes `browser-control-repro.dmm`, a 40x28 map containing
the stable turf and area. This makes the map eye and its contents explicit in
the compiled world, rather than relying on an empty generated world.

### Active reproducer work — 2026-08-17

The standalone source was restored outside the Wine checkout at:

```text
/home/octavio/Downloads/linux-ports/repro/dreamseeker-webview2/
```

The first version was a valid negative control: it used a real D3D9 map,
four WebView2 documents, desktop-to-game `put_ParentWindow` calls, and real
CSS hover events, but the HTML remained stable. Two fidelity corrections were
then made from the real DreamSeeker trace:

1. The 1258x696 login surface is centered at `x=54` inside the 1366x696 game
   client, rather than starting at `x=0`.
2. The ad-to-game lifecycle now creates the ad WebView first, hides it after a
   local navigation, retains its controller/renderer, and only then creates
   and reparents the four game WebViews. This gives the test five WebView2
   surfaces, matching the five renderer processes seen in the real run.
3. The host now uses a decorated frame containing a `WS_CHILD` game client
   with style `0x56010000`, matching the real DreamSeeker parent HWND. The
   centered login container is explicitly placed above the full-size document
   surface, matching the observed child stacking order.

The sequential ad-first run completed with all five controller callbacks and
all five navigation callbacks returning success. The ad controller hid
cleanly, and the game HTML surfaces received hover events without flickering.
This is still a negative control: it has not reproduced the SOA symptom, and
no Wine source was changed. The next reproducer work must compare the remaining
presentation/lifetime difference against the real trace before any Wine patch
is attempted. The latest run's `[hit-test]` target was the centered HTML B
surface, so the test is no longer accidentally measuring the full-size HTML A
surface.

## Real-game ad fast-forward — 2026-08-17

The retained runner now supports `BYOND_SKIP_ADS=1`. It starts WebView2 with a
loopback DevTools port, finds DreamSeeker's real `www.byond.com/rsc/ad.html`
target, and evaluates the same `pager://ok?type=static` navigation used by
the advertisement's Continue action. This removes the countdown without
modifying the BYOND prefix or Wine.

Validation run:

```text
test-results/20260817-011158-game
```

The real ad target was found at approximately 7 seconds, and the full
DreamSeeker game surface appeared before the old 50-second transition. This
is a timing convenience for the real-game integration test, not a rendering
fix and not a standalone reproducer.

### What happened to the claimed Wine fix?

There is no claimed fix in the current Wine checkout. The checkout is clean
upstream at commit `e99fc2f` (`mshtml: Don't leak the attribute after
forwarding in setAttributeNode.`), and the external 32-bit install is simply a
build of that source. The old experimental changes were reverted or left in
historical test records. The corrected external runner fixed the *test
layout/path* problem; it did not change Wine's compositor behavior. Any earlier
wording that called it a Wine fix was incorrect.

## Working configuration baseline

Installed in the prefix:

- `vcrun2022`
- `dxtrans`
- `corefonts`
- Microsoft Edge WebView2 32-bit runtime `109.0.1518.140`

Important compatibility settings:

- Prefix architecture: `win32`
- Default Windows version: Windows 7
- `msedgewebview2.exe`: Windows 7
- `dreamseeker.exe`: Windows 7
- `StrictDrawOrdering`: enabled
- DXVK: disabled; WineD3D is used
- `d3d11`, `dxgi`, `d3d9`, and `d3d12`: Wine builtin DLLs
- X11-oriented environment:

  ```text
  GDK_BACKEND=x11
  SDL_VIDEODRIVER=x11
  XDG_SESSION_TYPE=x11
  ```

- `MicrosoftEdgeUpdate.exe` was renamed to `MicrosoftEdgeUpdate.exe.bak` inside the prefix to prevent its background updater from interfering with the test.

### Proposed WebView2 Windows-version split — hypothesis only

This is documented for the next isolated reproducer test; it has **not** been
applied to the working prefix or to Wine source.

| Process or scope | Proposed Wine version | Reason |
|---|---|---|
| Prefix default | Windows 7 | Preserve the known BYOND compatibility baseline |
| `byond.exe` | Windows 7 | Preserve Hub/Pager behavior while testing the game separately |
| `dreamseeker.exe` | Windows 10 | Allow the browser-control transparency path required by BYOND 516 |
| `msedgewebview2.exe` | Windows 10 | Keep the WebView2 helper on the same compatibility behavior |

BYOND 516.1680 added the browser-control `inner-background-color` parameter
and transparency support; version 516.1686 should include that feature:

- <https://www.byond.com/forum/post/2963117>
- <https://www.byond.com/docs/notes/516.html>

Microsoft's WebView2 documentation says that an alpha value other than 255
returns `E_INVALIDARG` on Windows 7 when `DefaultBackgroundColor` is used:

- <https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controller2?view=webview2-1.0.2535.41>

That matches the current direct WebView2 harness log (`0x80070057`) and its
large white surface. The result means the harness is asking for a capability
that its Windows 7 process profile rejects; it does **not** yet identify the
SOA hover-flicker root cause.

Expected interpretation of the real BYOND test:

- If the split makes the transparent browser panel appear correctly, it was a
  missing compatibility prerequisite.
- If the panel appears but still flickers on hover, the remaining problem is
  likely in Wine's presentation/composition path.
- If the BYOND test is stable, the reproducer still differs from SOA and Wine
  should not be patched based on it.

The persistent Lutris WebView2 argument string is:

```text
--disable-gpu --disable-gpu-sandbox --disable-gpu-compositing --disable-d3d11 --disable-dwm-composition --use-angle=d3d9 --disable-background-networking --disable-metrics --disable-metrics-repo --disable-crash-reporter --disable-component-update --disable-features=msEdgeMetricsReporting,msWebOOUI
```

### Important test-harness note

The persistent Lutris configuration does **not** include `--no-sandbox`. Earlier versions of `byond-test-runner.sh` did include it by default, which made those runs a different process-model experiment. The helper has now been aligned with the persistent configuration. Older behavior must be requested explicitly with `BYOND_WEBVIEW_ARGS`, and every controlled run must record the exact argument string.

## What has already been established

The problem is not simply that the HTML, server, or game map is broken:

- The DreamSeeker map renders cleanly.
- The WebView2 renderer creates the login DOM.
- The WebView2 controller can initialize in the successful runs.
- The login form appears intermittently, including in the captured reference image at `/tmp/codex-clipboard-35107f00-e927-4a0b-9d42-bf0a90c5aec0.png`.
- The failure is in the final presentation step: the browser surface and HTML are created, but Chromium's pixels do not consistently reach DreamSeeker's Direct3D surface. The browser's visible-looking X11 host window is intentionally created hidden by WebView2/Chromium; therefore `IsUnMapped` for that host is not, by itself, evidence that Wine forgot to show the form. The symptom looks like a z-order problem because the missing browser pixels leave the map visible underneath.

This is analogous to a document being written correctly but the projector failing to put the document on the screen.

## Controlled results so far

Evidence is stored under `/home/octavio/Downloads/linux-ports/test-results/`.

### BYOND Hub

Run: `20260812-173819-hub`

- `hub_frames=9`
- nonblank Hub frames were captured
- `controller_failure=NO`
- result: `PASS`

The Hub can render with the current workaround configuration, but blank/black launches remain possible and need regression testing.

### DreamSeeker

Run: `20260812-173910-game`

- `game_frames=17`
- map frames were nonblank
- `login_dom_seen=YES`
- `controller_failure=NO`
- `webview_unmapped_frames=47`
- `webview_viewable_frames=0`
- result: map/controller surface passed; embedded-form composition still failed visual review

The most useful interpretation is: WebView2 is alive and the HTML exists, but Wine is not presenting its window reliably over DreamSeeker.

### Other tested variants

- WebView2 Windows 8 override: no improvement; restored to Windows 7.
- `StrictDrawOrdering` disabled: no improvement; restored to enabled.
- Removing `--disable-gpu-compositing`: no improvement.
- `--disable-direct-composition`: did not reach the game; the run was interrupted by Wine Mono/rundll32 setup behavior.
- `--disable-features=CalculateNativeWinOcclusion`: reached the game and DOM, but the WebView remained unmapped.
- Wine virtual desktop: bad result; black advertisement surface and `0x800700cb`, no usable game frames.
- X11 `XMapRaised` manipulation: WebView disappeared and DreamSeeker terminated; not a fix.

These tests make a simple browser flag or focus workaround unlikely to be sufficient.

## All-Windows-10 configuration audit and upstream corroboration — 2026-08-19

The final configuration-only check used the isolated prefix
`/home/octavio/Downloads/linux-ports/byond32-upstream-baseline/` with the clean
upstream Wine build at `/home/octavio/Downloads/wine-build-upstream32/install`.
The Windows version was reported as `win10` globally and for `byond.exe`,
`dreamseeker.exe`, and `msedgewebview2.exe`. The run added WebView2 logging and
Wine tracing only; it did not enable a new rendering workaround.

The result was unchanged: the D3D9 map loaded, the HTML panel did not remain
visible, and clicking the expected panel location produced only a brief white
surface. Therefore, changing the reported Windows version to `win10` for all
four scopes is not sufficient to fix the DreamSeeker presentation failure.

### What the final logs establish

- The WebView2 log contains normal page activity, but also
  `Failed to send GpuControl.CreateCommandBuffer` errors and recorded
  `GPU.ContextLost.RenderCompositor` / `GPU.ContextLost.RenderWorker` events.
  There is no controller-creation or `RPC_E_DISCONNECTED` failure in this
  run. The HTML is being loaded; the failure is later in the graphics path.
- The Wine trace shows the browser child HWND `0x20148` being parented to
  `0x10140`. Its X11 window ID is `0`, which means Wine is treating it as a
  child surface inside the parent rather than as an independent top-level X11
  window. The child is shown, resized, hidden, and finally destroyed during
  application shutdown. It is not immediately deleted merely because the
  pointer moves over the HTML area.
- Upstream Wine's `dlls/dcomp/device.c` still returns `E_NOTIMPL` from
  `DCompositionCreateDevice2`. Upstream `dlls/d3d11/device.c` likewise returns
  `E_NOTIMPL` from `ID3D11Device::OpenSharedResource` and
  `ID3D11Device1::OpenSharedResource1`. These are not proof that BYOND calls
  those exact functions, but they show that important pieces of the Windows
  composition/shared-resource contract are still absent.
- The repeated `RoGetActivationFactory` messages concern optional Windows
  Runtime classes and feature probes. Registering two of those classes with
  `combase.dll` did not implement the missing factories and did not restore the
  pixels, so those messages are not being treated as the visual root cause.

### Independent Wine report matching this failure family

Wine Bug 59370, filed against Wine 11.1 with WebView2 runtime 109, reports
blank white/black WebView2 windows even though Chromium's renderer produces
content and DevTools Protocol screenshots are nonblank. Its analysis points to
the D3D11 shared-texture and DirectComposition handoff, including Chromium's
cross-process SharedImage/mailbox path, rather than to HTML or ordinary window
creation:

[Wine Bug 59370 — WebView2 109 blank window / D3D11 shared-texture path](https://list.winehq.org/hyperkitty/list/wine-bugs%40list.winehq.org/thread/UDZKZTMP5WYISOAUDGEOMFOSU7X73JND/)

That report says that several software-rendering, ANGLE, GPU, and
DirectComposition command-line workarounds failed for that reporter. A Wine
maintainer also noted that native WebView2 needs DirectComposition,
multiprocess surface support, and sandbox support. The report was later marked
as a duplicate of Bug 58921 and closed; “duplicate” means that the report was
redirected to an existing issue, not that the underlying defect was fixed.

There is also an independent Microsoft WebView2 report that demonstrates the
same important distinction on native Windows: the DOM and CDP screenshot stay
correct while the host HWND shows stale or black pixels, and transient activity
can briefly make the content visible. That report concerns a different trigger
(video-overlay damage tracking on Windows 11), so it is corroborating evidence
about the presentation boundary, not proof that BYOND has the same Microsoft
bug:

[WebView2 issue 5574 — surface paints correctly but the host window does not present it](https://github.com/MicrosoftEdge/WebView2Feedback/issues/5574)

### Current conclusion after this audit

The Windows-version experiment is now recorded as tested and insufficient. The
best-supported explanation is that WebView2/Chromium can build the HTML, but
Wine fails to deliver the browser's GPU/composited surface to DreamSeeker's
embedded presentation surface. The remaining source-level candidates are the
DirectComposition implementation and the cross-process D3D11/SharedImage
handoff. The X11 child-window lifetime issue remains real but is not sufficient
on its own, because preserving the child did not restore the HTML pixels.

This evidence justifies investigating Wine's composition/shared-surface code,
but it still does not justify claiming that implementing one particular stub
will fix BYOND. The next Wine change must be narrow, measurable, and tested
against this exact log/screenshot flow.

## Wine-Staging 11.14 comparison — 2026-08-19

The next single test used the same isolated prefix and the same SOA server, but
ran the Lutris Wine-Staging 11.14 runner:

`/home/octavio/.local/share/lutris/runners/wine/wine-11.14-staging-x86_64`

No WebView2 rendering workaround arguments were enabled. WebView2 logging and
focused Wine D3D/DirectComposition tracing were enabled. The complete run is
under:

`/home/octavio/Downloads/linux-ports/test-results/20260819-175634-game/`

The advertisement transition completed at about 51 seconds and the D3D9 map
and title screen rendered. The HTML login panel was not visible in the
post-advertisement captures. DreamSeeker exited at about 94 seconds, before
the requested 110-second observation window. This is recorded as an abnormal
early termination; this run alone does not distinguish a crash from the game
or server closing the process.

### What staging changed

Unlike the clean upstream run, Wine-Staging reached its experimental
DirectComposition object path. The trace contains `visual_AddVisual`,
`visual_SetContent`, and repeated `device_Commit` calls. This is consistent with
the [Wine-Staging DirectComposition patchset](https://github.com/wine-staging/wine-staging/commit/b70caa17726c3532b210a5ddf53af8024bc35b34)
being active; staging is not simply behaving like upstream `E_NOTIMPL`.

However, the same trace labels `visual_SetContent` and `device_Commit` as
semi-stubs, reports an unsupported DirectComposition interface query, and
reports missing D3D11/DXGI texture interfaces. Most decisively, WebView2's
ANGLE layer logs:

`Could not get offscreen texture shared handle, HRESULT: 0x80004001`

`0x80004001` is `E_NOTIMPL`: the browser asked for a shared GPU texture handle
and the Wine path still did not provide the operation. Therefore, staging gets
farther into DirectComposition but does **not** fix the BYOND failure. The map
remaining visible while the HTML remains absent narrows the problem toward
the browser's D3D11/shared-texture handoff, not DreamSeeker's D3D9 map drawing.

This test changed neither Wine source nor the prefix. Its artifacts are the
evidence for the next source-level investigation; no additional workaround
experiment was started.

## Committed BYOND browser-control reproducer under Wine-Staging — 2026-08-19

The committed BYOND reproducer was then run by itself; SOA was not involved:

`/home/octavio/Downloads/linux-ports/repro/byond-browser-control/`

It used the same isolated prefix and the Lutris Wine-Staging 11.14 runner. The
run was allowed to continue for 120 seconds for log collection and was then
stopped by the timeout (`status=124`); this is a test-harness timeout, not a
DreamSeeker crash result. The artifacts are under:

`/home/octavio/Downloads/linux-ports/test-results/20260819-staging-byond-browser-control/`

This reproducer reaches the same underlying graphics failure family:

- D3D11 initializes and performs feature/format probes; it does not fail at
  device creation.
- DXGI reports `dxgi_resource_GetSharedHandle ... stub!`, meaning Wine does
  not provide the texture-sharing operation requested by the browser.
- The texture's keyed-mutex interface query returns `E_NOINTERFACE`.
- DXGI reports `CreateSwapChainForComposition ... stub!`.
- Wine-Staging then reaches DirectComposition, but
  `visual_SetContent` and `device_Commit` are still marked `semi-stub`.

The important log sequence is therefore not “D3D11 failed instead of
DirectComposition.” D3D11/DXGI fails to create the shareable presentation
resource first; DirectComposition is reached afterward, but cannot complete
the composition because its content and commit operations are incomplete.
The native reproducer did not enable Chromium's `--enable-logging` argument,
so it does not print ANGLE's higher-level
`Could not get offscreen texture shared handle` message. The Wine trace shows
the lower-level API failures that produce that message.

This is the first reproducer run that directly confirms the staging result
without relying on SOA's long loading sequence. It is a valid source-level
failure, not an artificial hover effect.

## Step 1: deterministic graphics API probe created — 2026-08-21

The first step of the source-fix plan is now complete. The new 32-bit probe is
at:

`/home/octavio/Downloads/linux-ports/repro/wine-composition-api/`

It creates D3D11 shareable textures, requests legacy and NT shared handles,
queries keyed-mutex/resource interfaces, attempts D3D11 handle reopening,
creates a composition swap chain, and calls DirectComposition device/visual
operations. It prints each HRESULT and does not launch BYOND, WebView2, or
generate artificial flicker.

The probe builds successfully as a PE32 executable with
`i686-w64-mingw32-gcc`. It has not been run against either Wine runner yet;
that is the next control step. The local `dcomp-compat.h` file exists only
because the installed MinGW `dcomp.h` is not C-compilable in this environment;
it declares the small ABI subset used by the probe and does not modify Wine.

## Latest BYOND reproducer log analysis — 2026-08-23

The real BYOND browser-control reproducer was launched with the clean upstream
Wine build and the isolated prefix. Visually, it still showed the established
failure: the native map remained visible, the HTML panel did not remain
visible, and clicking the expected panel location produced a brief white
surface. The run log is:

`/tmp/byond-browser-control-gpu-native-record.log`

### What this run proves

The failure is not a simple “WebView2 never started” or “Wine destroyed the
HTML HWND” failure:

- DreamSeeker reached the `HANDOFF_SENT` state.
- The WebView2 page reached `JS_WINSET_SENT`, so the HTML document loaded and
  its JavaScript executed.
- The X11 tree contained the browser window titled
  `BYOND HudLib | JS_WINSET_SENT | target=400x330`.
- `xwininfo` reported that window as `Map State: IsViewable`, with a valid
  parent, a `1100x734` client surface, and process id `1713398`.
- WebView2 manager, renderer, GPU, D3D11, DXGI, and DirectComposition activity
  appeared in the Wine log. `DCompositionCreateDevice2` was called at log
  line 765.

In plain language: the browser document and its window exist, but the image
buffer produced by the browser is not being presented correctly. The best
current model is “the browser painted the canvas, but Wine did not put that
canvas into the visible DreamSeeker composition,” not “the canvas was
deleted.” `IsViewable` proves that X11 considers the window mapped; it does not
prove that the window contains valid pixels.

### What the warnings suggest, without proving

The same log contains the following relevant signals:

| Evidence | Count or location | Interpretation |
| --- | ---: | --- |
| `dcomp:DCompositionCreateDevice2` | line 765 | The DirectComposition path is entered; the `fixme` status means the implementation is incomplete, not that this call necessarily failed. |
| `d3d11_device_GetDeviceRemovedReason` | 48 | Wine does not implement a normal device-loss status query. This is a candidate for browser compositor confusion, but the log does not show the returned HRESULT. |
| `dxgi_adapter_QueryInterface ... not implemented` | 4 | WebView2/ANGLE asks for a DXGI interface that this Wine path does not expose. The exact caller-side fallback is unknown. |
| `unexpected _NET_ACTIVE_WINDOW` | 38 | Wine is seeing repeated focus/activation changes. This supports a window-ordering race, but does not by itself explain missing pixels. |
| `wined3d_swapchain_resize_buffers ... still holding back buffer` | 4 | The D3D9 map is being resized while a back buffer is referenced. This may contribute to lag or map corruption, but is not enough to explain the HTML-only failure. |

The optional Windows Runtime factory errors, including
`WebAuthenticationCoreManager`, are not the visual root cause in this run:
the page reached JavaScript after those errors. They remain compatibility noise
unless a future run shows controller creation stopping before `JS_WINSET_SENT`.

`webview-debug.txt` contains one controller error:

`WebView2 failed to create a controller, with error code 0x8007139f`

`0x8007139f` is the HRESULT form of Windows `ERROR_INVALID_STATE`. However,
that file was last modified at 21:51:56, while the latest Wine log was
modified at 22:03, and the file contains only one overwritten line. It must
therefore be treated as evidence that one controller-creation attempt failed,
not as a synchronized explanation for every browser surface in the latest
run. The current log does not contain the earlier `0x80010108`
(`RPC_E_DISCONNECTED`) message.

### Test-quality warning

The X11 tree showed two reproducer/DreamSeeker instances at once. That makes
the activation warnings and surface ownership harder to interpret and may
itself amplify z-order churn. A clean follow-up must terminate every process
in the isolated test prefix, launch exactly one world, and then inspect one
browser HWND. This is test cleanup, not a proposed rendering workaround.

### Current conclusion

This run falsifies the narrow theory that Wine simply deletes the embedded
browser window during parenting or hover. It strengthens the broader
cross-process D3D11/shared-surface and DirectComposition hypothesis, while
also identifying focus/activation churn as a secondary issue to measure. The
logs still do not identify the exact missing API or prove which Wine function
needs to change.

## Native Wayland smoke test — 2026-08-26

The first Wayland comparison was run with the stock Lutris Wine-Staging 11.14
runner, because the experimental 32-bit Wine build at
`/home/octavio/Downloads/wine-build-upstream32/install` contains
`winex11.drv` but does not contain `winewayland.drv`. The test used the clean
BYOND baseline prefix and the external DreamSeeker/WebView2 reproducer; no
Wine source or working prefix was changed.

The launch explicitly unset `DISPLAY` and set:

```text
WAYLAND_DISPLAY=wayland-0
XDG_SESSION_TYPE=wayland
GDK_BACKEND=wayland
SDL_VIDEODRIVER=wayland
```

The Wine loader trace confirmed that `winewayland.drv` was loaded by the
reproducer and WebView2 processes. Earlier captures containing “Xwayland” do
not count as this test: those used Wine's X11 driver through the Xwayland
compatibility layer.

The screenshot looked like Wine virtual-desktop mode because the windows had
Windows-style borders and the advertisement window appeared inside the same
screen area. It was not the Wine virtual-desktop feature: the reproducer was
launched directly, not through `explorer.exe /desktop=...`, and the prefix has
no `Software\\Wine\\Explorer\\Desktops` configuration. The apparent layout is
the native Wayland window presentation and the reproducer's separate
advertisement top-level window.

The D3D9 map was visible, but the advertisement WebView was a large blank
white window and the embedded game HTML panel never appeared. The WebView2
log is:

`/tmp/byond-wayland-webview2-user-data/EBWebView/chrome_debug.log`

It reports repeated:

```text
GPU process launch failed: error_code=2
```

followed by nine failed attempts and:

```text
GPU process isn't usable. Goodbye.
```

The reproducer creates its game WebView2 controllers only after the
advertisement navigation succeeds. Therefore this run did not reach the
embedded HTML or hover-flicker stage. It is a Wayland/WebView2 GPU-startup
failure, not a result about DirectComposition ordering and not a reproduction
of the SOA symptom.

The next controlled comparison is native Wayland with the same WebView2
software-rendering arguments used by the known baseline. If the advertisement
and game panel appear, we can then judge whether Wayland changes the actual
composition behavior. If they do not, Wayland cannot reach the relevant
flicker test with WebView2 109 in this setup.

### Native Wayland software-rendering follow-up — 2026-08-26

The follow-up uses the same stock Lutris Wine-Staging 11.14 runner, baseline
prefix, and native Wayland environment, but supplies the known software/ANGLE
arguments:

```text
--disable-gpu --disable-gpu-sandbox --disable-gpu-compositing
--disable-d3d11 --disable-dwm-composition --use-angle=d3d9
--disable-background-networking --disable-metrics --disable-metrics-repo
--disable-crash-reporter --disable-component-update
--disable-features=msEdgeMetricsReporting,msWebOOUI --enable-logging
```

The run uses a separate temporary WebView2 profile at
`/tmp/byond-wayland-webview2-software`, so it does not reuse the previous
native-GPU run's browser state. The browser did select a SwiftShader GPU
process and the D3D9 host remained visible, but the advertisement stayed a
blank white window. The reproducer never printed the five-surface
navigation-complete message, and no embedded game panel was created. The
process later ended with status 0 without reaching the hover test; that is not
a pass.

This result does not mean that HTML requires D3D11 for its own layout or CSS.
It means that disabling D3D11 did not produce the browser surface that this
WebView2 embedding path needs. The option is therefore rejected as a fix and
must not be used in the real DreamSeeker comparison. The browser's normal
Windows path should be allowed to request D3D11, shared textures, and
DirectComposition; those are exactly the interfaces Wine must implement or
repair.

## Configuration-only tests still worth doing — ordered, not parallel

Most simple configuration ideas are already negative results: Windows 10
version overrides, `StrictDrawOrdering`, removing
`--disable-gpu-compositing`, virtual desktop mode, and X11 raising/topmost
workarounds did not restore the form. The current Lutris profile also forces
WebView2 toward a software/ANGLE/D3D9 path with:

```text
--disable-gpu --disable-gpu-sandbox --disable-gpu-compositing
--disable-d3d11 --disable-dwm-composition --use-angle=d3d9
```

The reproducer run above was launched without `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS`,
so it already exercised the native WebView2 GPU/D3D11 path and still failed.
That means simply removing the Lutris software flags is not expected to be a
complete fix, although it remains useful to compare against the real game
when the server is available.

The final configuration experiment with clear diagnostic value was Chromium's
in-process GPU mode. It reduces the browser-to-GPU-process boundary that
normally requires a cross-process shared texture, so it tests the leading
hypothesis without modifying Wine:

```bash
cd /home/octavio/Downloads/linux-ports/repro/byond-browser-control

WINEPREFIX=/home/octavio/Downloads/linux-ports/byond32-upstream-baseline \
WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS='--in-process-gpu --enable-logging --v=1' \
./run.sh 2>&1 | tee /tmp/byond-browser-control-in-process-gpu.log
```

The run was made after the isolated prefix was cleaned. The visual result was
unchanged: the map remained visible, the HTML panel remained absent, and the
brief white surface still appeared. Its log is:

`/tmp/byond-browser-control-in-process-gpu.log`

The log still entered `DCompositionCreateDevice2`. It does not contain a
browser-side confirmation that Chromium honored `--in-process-gpu`, so this is
a negative configuration result but not a mathematically perfect proof that
every renderer process boundary disappeared. It is nevertheless enough to
reject in-process GPU mode as a practical workaround.

Interpret the result this way:

- If the HTML becomes stable, the cross-process GPU/shared-surface boundary is
  strongly implicated. This is a diagnostic result, not a production setting;
  in-process GPU mode changes WebView2's process isolation and may be unsafe or
  unsupported.
- The unchanged result means the failure is probably later at the DXGI/
  DirectComposition/host-window presentation boundary, and more browser flags
  are unlikely to be a production solution.

The previously considered second diagnostic flag, `--no-sandbox`, is not
being promoted to a fix. It changes WebView2's security/process model and the
current evidence does not point to sandbox startup as the presentation failure.
No Windows-version, z-order, or topmost workaround should be added to the
production profile based on a temporary visual improvement.

## Likely Wine source-level fix shape — 2026-08-23

The configuration path is now exhausted enough to begin source work, but the
exact patch is not yet known. The clean upstream tree is still unchanged at
`/home/octavio/Downloads/wine-upstream/` and the current source-level targets
are generic Wine interfaces, not BYOND-specific code.

The current evidence points to three layers of missing plumbing:

1. **Cross-process GPU resource sharing.** A browser renderer produces a GPU
   texture in one Wine process, while the host/compositor consumes it in
   another. Wine needs a real shared-resource identity and handle path. That
   means implementing the relevant `IDXGIResource`/`IDXGIResource1` shared
   handle operations, and making `ID3D11Device::OpenSharedResource()` and
   `OpenSharedResource1()` reopen the same resource in the receiving process.
   The implementation must preserve format, dimensions, access rights,
   lifetime, and synchronization; returning a locally valid but unrelated
   texture would only hide the bug.
2. **DirectComposition object behavior.** `DCompositionCreateDevice2()` must
   return a usable `IDCompositionDevice2` object rather than `E_NOTIMPL`, with
   visuals/surfaces, content assignment, and `Commit()` semantics. The DXGI
   side also needs a usable `CreateSwapChainForComposition()` path so a
   browser-produced swap chain can become DirectComposition content.
3. **Host presentation integration.** Once the compositor has a valid surface,
   Wine must connect the committed surface to the correct embedded Win32/X11
   child, propagate damage/expose notifications, and preserve the normal
   parent/child lifetime. It must not raise a root-level overlay or make the
   WebView globally topmost.

The relevant upstream stubs are visible in:

- `dlls/dxgi/resource.c`: `GetSharedHandle()` and `CreateSharedHandle()`;
- `dlls/d3d11/device.c`: `OpenSharedResource()`,
  `OpenSharedResource1()`, and the device-status query;
- `dlls/dxgi/factory.c`: `CreateSwapChainForComposition()`;
- `dlls/dcomp/device.c`: all three `DCompositionCreateDevice*()` entry points.

The old X11 reparenting issue remains a real generic bug candidate, but it is
not the first source target now: the latest reproducer showed a mapped,
viewable WebView window whose pixels were absent. The first implementation
milestone should therefore be a small shared-resource/D3D11 test that passes
between two Wine processes. Only after that passes should the DirectComposition
object model and X11 presentation wiring be expanded.

This is a source-level design direction, not a claimed fix. The next coding
step must be one narrow implementation plus the existing D3D11/DXGI tests;
the BYOND reproducer remains the integration test.

## Branch and merge-request plan — 2026-08-24

We will not create three branches and run three experiments at the same time.
There are three possible pieces of work, but the third is only a possibility
until the first two have been tested.

```text
upstream master
├── codex/d3d11-shared-resources   MR 1: first branch
│   └── codex/dcomp-composition    MR 2: likely depends on MR 1
└── codex/winex11-child-lifetime   MR 3: independent; create only if proven necessary
```

The names are local branch names. The `codex/` prefix is only a namespace to
make these branches easy to recognize; it is not a special Wine requirement.

1. **`codex/d3d11-shared-resources` is first.** It will contain only the
   generic DXGI/D3D11 shared-resource behavior and its Wine tests. The first
   test will pass a shared resource between two Wine processes. It must fail
   on the clean upstream build and pass after the change before we continue.
2. **`codex/dcomp-composition` comes second only if needed.** DirectComposition
   needs a usable resource to present, so this branch will normally be based
   on the first branch while it is being tested. If the tests show that the
   two pieces are independent, it can later be retargeted to upstream
   `master` instead.
3. **`codex/winex11-child-lifetime` is deferred.** It would contain a generic
   X11/Win32 child-window lifetime fix, but we will create it only if a focused
   test proves that the child-window lifetime is still wrong after the
   resource/composition path is working. It starts from upstream `master`, so
   it is independent of the graphics branches.

Each branch must stay generic: no BYOND executable names, game coordinates,
topmost overlays, or environment-variable workarounds. The real DreamSeeker
run is the integration test after each small Wine test passes. We are creating
only the first branch now.

## Roadmap — one experiment at a time

1. **Document the problem and freeze the controls.** Keep the working Lutris
   runner and `/home/octavio/Games/byond32` unchanged. **Complete.**
2. **Build and record a clean upstream Wine baseline.** Use one external Wine
   source tree and one separate build/install directory. **Complete.**
3. **Add and run the focused Wine regression test.** The parent process creates
   a shared D3D11 texture and the child process reopens it. **Complete as a
   baseline:** on clean upstream Wine, both the legacy shared-handle path and
   the newer NT shared-handle path return `E_NOTIMPL`. Those are recorded as
   expected baseline failures, not passing implementations.
4. **Implement the first generic shared-resource path.** The experimental
   branch now exports and reopens a legacy shared resource successfully when
   WineD3D uses its Vulkan path. The same native path used by the normal test
   still reports the legacy operation as unimplemented. Keyed mutex support
   and the newer NT-handle path are also missing. This is a partial handoff
   checkpoint, not a complete shared-resource implementation or a presentation
   fix. **Partial only.**
5. **Run Gate A with BYOND and DreamSeeker.** Check for regressions and record
   whether the login HTML changes. The full Gate A procedure is below.
   **Complete; the normal-renderer run stayed alive but the HTML layer was
   still missing.**
6. **Implement the DirectComposition presentation path** on the dependent
   `dcomp-composition` branch, only if the shared-resource work does not
   finish the presentation path by itself. **Complete for the bounded
   visual-tree slice on 2026-08-28; the full presentation path remains
   incomplete.** Visual offsets/translations are retained and the compositor
   now visits every content visual in tree order. The branch still has a WIP
   GDI copy bridge rather than a proper composition surface, and it still
   lacks complete transforms, alpha semantics, and commit/presentation
   behavior. The next required part of this step is the real
   DirectComposition surface-to-window presentation path; the internal GDI
   copy result is not enough.
7. **Run Gate B with BYOND and DreamSeeker.** Check the actual login and
   inventory/HudLib surfaces for stable layering, clean removal, focus
   isolation, and five-minute stability. The first Gate B checkpoint is
   complete but negative: the applications stay alive, while the HTML surface
   remains detached from the game composition. The full Gate B procedure is
   below.
8. **Trace the X11 child-window lifetime after Gate B fails.** The trace
   confirmed that Wine destroyed the real browser child during reparenting.
   **Complete.**
9. **Test the narrow X11 child-window preservation change.** The browser
   windows now survive and are nested below DreamSeeker, but the pixels were
   still absent. **Complete as a structural checkpoint; not a visual pass.**
10. **Implement and validate the DirectComposition surface-to-window pixel
    handoff.** **Investigation complete; implementation not complete.** The
    trace found both a hidden `Intermediate D3D Window` case and a live target
    where the GDI copy returned `copied 1`. The latter is only an internal copy
    result; it did not prove correct pixels on screen. The remaining source
    work is to present the WebView2 composition surface to the correct visible
    host while preserving alpha, ordering, and multiple visual surfaces. The
    hidden-destination guard is a completed safety substep, not the required
    surface-to-window implementation.
11. **Run Gate C with DreamSeeker and the Hub on the restored positive path.**
    Manually check embedded-panel visibility, form removal after login, focus
    isolation, and the inventory or HudLib surface. Then run the long stability
    check. **Still failed:** manual verification shows that the entire embedded
    HTML panel can flicker or disappear. The old per-control hover flicker is
    not currently observed and predates the latest source changes, so it is not
    evidence of a recent fix. The separate backup login window is not a valid
    Gate C result. The remaining DirectComposition presentation work must
    continue before Gate C is retried.
12. **Prepare the final review material.** Keep each logical Wine change
    separate, remove temporary trace instrumentation, rerun the Wine tests
    and all BYOND gates, and document exactly which generic behavior changed.
13. **Ensure `SOA.exe` works on Linux.** This means testing the exact SOA +
    BYOND executable supplied by the SOA team with the finished Wine build,
    including startup, map rendering, embedded login and inventory/HudLib
    panels, focus isolation, clean panel removal, and a long stability run.
    **Not started.** This step also requires the SOA team to confirm which
    files may legally be redistributed.
14. **Package `SOA.exe` as one Linux-friendly artifact.** **Not started.**
    Compare all realistic delivery formats before choosing one:

    - **AppImage:** one executable file that can contain a launcher, the
      patched Wine build, and a prepared prefix. This most closely matches the
      request for one downloadable file. It will be large and still depends on
      the host kernel, graphics driver, and suitable 32-bit host libraries.
      See the [AppImage concepts documentation](https://docs.appimage.org/introduction/concepts.html).
    - **Portable archive with a launcher:** one compressed archive or
      self-extracting file that creates a private prefix on first launch. This
      is simple to build and can support user-provided files, but it needs a
      custom update and desktop integration story.
    - **Lutris YAML installer:** a setup recipe that selects the exact Wine
      runner, creates the 32-bit prefix, installs permitted dependencies, and
      asks the user to provide files that must not be bundled. It is not a
      self-contained application, but it is the safest choice when SOA.exe,
      BYOND, or WebView2 cannot be redistributed. See the [Lutris installer
      documentation](https://github.com/lutris/lutris/blob/master/docs/installers.rst).
    - **Flatpak:** a sandboxed application with a controlled runtime and
      dependencies. A Flatpak bundle can be one file, but the runtime and
      other dependencies may still be separate downloads. The sandbox also
      needs explicit permissions for files, graphics, and processes. See the
      [Flatpak bundle documentation](https://docs.flatpak.org/en/latest/single-file-bundles.html)
      and [Flatpak dependency documentation](https://docs.flatpak.org/en/latest/dependencies.html).
    - **Debian package:** a good choice for Debian users because dependencies
      can be declared, but it is not distribution neutral and is less suitable
      for the requested single-file experience.
    - **Bottles recipe:** useful for users who already use Bottles, but it
      depends on Bottles and its runner management. It is not an independent
      SOA package. See the [Bottles runner documentation](https://docs.usebottles.com/components/runners).

    The current recommendation is to test an AppImage and a portable launcher
    against the Lutris installer. If redistribution permission is missing,
    the Lutris installer or portable launcher must download or request the
    restricted files. If permission exists and the self-contained test passes,
    AppImage is the strongest one-file candidate. Flatpak remains the strongest
    isolation candidate, not the automatic winner. Every option must use a
    fresh prefix, leave the user's existing BYOND and Lutris setup untouched,
    and record the exact Wine build and WebView2 requirements.

### BYOND/DreamSeeker integration gates

The small Wine test is the first decision point, but it is not enough by
itself. We will run the real applications at the integration gates in the
roadmap. These are validation gates, not separate branches or parallel
experiments.

**Gate A — after the shared-resource branch (roadmap step 5).**

Use the same isolated Wine install and the separate BYOND prefix used by the
Wine test. Do not change the working Lutris runner or `/home/octavio/Games/byond32`.

1. Start `byond.exe` and confirm that the Hub loads `pagerhome.html` instead
   of showing a blank or black HTML area. Record the WebView2 debug log and
   whether the Hub remains responsive.
2. Start DreamSeeker normally against the real BYOND server, wait through the
   normal advertisement, and confirm that the map and login DOM are created.
3. Record what the login form does at this stage: fully visible, briefly
   visible, missing, or visible only when the pointer moves over it. This
   result may still be negative because DirectComposition has not been changed
   yet; the purpose of Gate A is to detect regressions and measure whether the
   shared-resource change altered the behavior.
4. Continue for at least two minutes after the map appears and record any
   crash, hang, or new white rectangle. If the real server is unavailable,
   record the integration gate as **blocked**, rather than treating the
   committed reproducer as proof that DreamSeeker works.

We continue to the DirectComposition branch only if the two-process Wine test
passes and the Hub/DreamSeeker run introduces no new regression. A missing
login form at Gate A is recorded as evidence for the next layer, not silently
called a success.

### Gate A result — 2026-08-24

The test used the isolated Wine build at
`/home/octavio/Downloads/wine-build-upstream32/install` and the separate prefix
`/home/octavio/Downloads/linux-ports/byond32-upstream-baseline`. The normal
Lutris runner and `/home/octavio/Games/byond32` were not changed.

The Hub run completed for 30 seconds. WebView2 loaded
`file:///C:/users/octavio/Documents/BYOND/pager/custom/pagerhome.html`, fetched
the Hub images, and kept its browser, GPU, renderer, and utility processes
alive. The run used telemetry-only capture, so this proves that the page was
loaded by the browser but does not by itself prove what the user saw on the
screen.

The DreamSeeker run used Wine's normal renderer (the Vulkan selector was not
set), waited through the automatic advertisement transition at about 51
seconds, and stayed alive for the full 95-second run. The map window was
present, and WebView2 created its browser/GPU/renderer processes. At the final
window-tree sample, the main WebView2 X11 surface was a separate root-level
`1016x542` window at `+4+30`, alongside a `DXGI device window`, rather than a
child listed below the DreamSeeker map window. Wine also logged
`DCompositionCreateDevice2` as a stub. This is consistent with the missing
composition/presentation layer, but it is not yet proof that every WebView2
surface uses the same path.

For completeness, a separate run that forced `WINE_D3D_CONFIG=renderer=vulkan`
is not a valid Gate A result: DreamSeeker reached the map but then crashed in
Wine's Vulkan D3D9 path at `wined3d_sampler_vk_cs_init`. The normal-renderer
run above is the control we use for the BYOND result.

Evidence is kept outside the project so it cannot pollute the Wine diff:

- `/tmp/byond-gate-a/20260824-192917-hub`
- `/tmp/byond-gate-a/20260824-193533-game`

Gate A therefore found no new normal-renderer crash, but it did not fix the
HTML layering problem. The next step is the DirectComposition trace and the
smallest real composition-object/lifetime implementation needed by the
WebView2 path.

**Gate B — after the DirectComposition branch (roadmap step 6).**

Repeat the same Hub and DreamSeeker run with the same prefix, WebView2 runtime,
Windows-version overrides, graphics backend, and browser arguments. This time,
after the normal advertisement:

1. Confirm that the map remains visible while the login form is visible above
   it.
2. Move the pointer repeatedly over the login controls and open the in-game
   inventory/HudLib browser surface if the server exposes it. The controls
   must remain stable; repainting one button must not erase the rest of the
   HTML surface or the map.
3. Fill in and submit the form, then confirm that the browser surface is
   removed cleanly rather than leaving a white rectangle.
4. Keep the game open for at least five minutes and switch focus to another
   application. The HTML surface must stay inside DreamSeeker and must not
   appear above unrelated applications. Record any lag, hang, or crash even
   though the primary Gate B decision is the flicker/layering result.

Gate B passes only when the generic Wine tests pass and the real HTML surfaces
are stable in DreamSeeker. If the form is still missing or flickers after Gate
B, only then do we investigate the deferred X11 child-window branch.

**Gate C — manual validation of the first positive pixel handoff (failed — 2026-08-28).**

The current isolated runner is the restored positive path: it retains the
generic X11 child-window preservation and the DirectComposition visible-host
handoff, while leaving the rejected opaque-alpha rule disabled. Launch it with
no extra WebView2 flags so the manual result tests the same native path as the
captured run:

```bash
cd /home/octavio/Downloads/linux-ports
env -u WINE_D3D_CONFIG \
  BYOND_PREFIX=/home/octavio/Downloads/linux-ports/byond32-upstream-baseline \
  BYOND_RUNNER=/home/octavio/Downloads/wine-build-upstream32/install \
  BYOND_WEBVIEW_ARGS='' BYOND_SKIP_ADS=0 BYOND_WINEDEBUG=-all \
  WINEDLLOVERRIDES='d3d11=builtin;dxgi=builtin;d3d10core=builtin;dcomp=builtin;winevulkan=builtin' \
  ./tools/byond-test-runner.sh --game byond://198.166.109.89:3336 --interactive
```

The manual pass/fail questions are deliberately limited to the original bug.
The 2026-08-28 run failed this gate: the recording still showed that the
internal copy result did not become a stable, correctly layered HTML surface.
The result must not be called a fix or a pass.

The manual pass/fail questions are:

1. After the advertisement, does the login form remain continuously visible
   while the map remains visible around it?
2. Does moving the pointer across every control leave the controls intact,
   without the hover repaint erasing or flickering them?
3. After login, does the form disappear without leaving a white rectangle?
4. Does switching focus to another application leave the HTML inside
   DreamSeeker rather than making it a desktop-wide topmost window?

The automated positive capture reached question 1 but cannot answer questions
2–4. Its final image is
`/tmp/byond-dcomp-parent-normal/20260824-204935-game/screens/game-0005.png`.

### DirectComposition slice and Gate B result — 2026-08-24

The first DirectComposition source slice was applied without committing it. It
adds the basic device, desktop-device, target, visual, `SetContent`, `SetRoot`,
and `Commit` objects, and builds Wine's `dcomp.dll` and `dxgi.dll` in the
isolated 32-bit runner. It is not yet a complete compositor: the source still
contains the staging `CreateSwapChainForComposition` window bridge and several
semi-stubs. The normal BYOND prefix and Lutris runner were not modified.

The controlled API probe, forced to use Wine's built-in DLLs, now reports
success for the basic chain:

```text
IDXGIFactory2::CreateSwapChainForComposition  S_OK
DCompositionCreateDevice2                  S_OK
CreateTargetForHwnd                         S_OK
CreateVisual                                S_OK
IDCompositionVisual::SetContent             S_OK
IDCompositionTarget::SetRoot                S_OK
IDCompositionDevice::Commit                 S_OK
```

The Hub run completed for 30 seconds and WebView2 loaded `pagerhome.html`.
The DreamSeeker run crossed the advertisement at about 50 seconds and stayed
alive for the full 95-second run. However, its Wine trace still contains
`dcomp:visual_AddVisual ... stub!` and an unsupported DirectComposition
interface query. The final X11 tree still shows the main WebView2 surface as a
separate root-level `1016x542` window instead of a surface presented through
the DreamSeeker map. This means the first slice improves API availability but
does not fix the HTML pixels or layering.

The next source slice is therefore the presentation tree itself: implement the
visual-child relationship and the actual content/presentation path used by
WebView2, then rerun the small probe and Gate B. We should not treat the
successful `S_OK` values above as proof that composition works; they only prove
that object creation and bookkeeping no longer stop immediately.

Evidence:

- `/tmp/byond-gate-b/20260824-195158-hub`
- `/tmp/byond-gate-b/20260824-195430-game`
- `/home/octavio/Downloads/linux-ports/repro/wine-composition-api`

### Visual-tree child-content fix and DreamSeeker result — 2026-08-24

The next source slice added ordered child visuals and COM ownership in
`dlls/dcomp/visual.c`. The compositor in `dlls/dcomp/device.c` now searches
the visual tree for content instead of requiring the root visual itself to
hold the content. This matches the WebView2 shape observed in the trace: a
root visual with a content-bearing child visual.

This changed the Hub result. The isolated Hub capture now shows the real
`pagerhome.html` interface instead of a blank white surface:

- `/tmp/byond-dcomp-childcontent/20260824-201007-hub/screens/hub-0005.png`

The Hub log no longer reports `visual_AddVisual ... stub!`; it reaches
`SetContent` and `Commit`. This is the first source change that visibly fixes
one of the two original failures, although the DirectComposition code is
still an experimental slice and has not been committed.

DreamSeeker was then run with the same built-in Wine DLL overrides for 95
seconds. It crossed the advertisement, stayed alive, and no longer reported
the `AddVisual` stub. Its log reaches `SetContent` and `Commit`, but the HTML
is still not part of the captured DreamSeeker map window. The final X11 tree
contains separate root-level WebView2 windows (`1366x742`, `1258x742`, and
`1016x542`) while the DreamSeeker map is a different nested window. This is
consistent with the remaining failure being in Wine's X11 parent/child
attachment or lifetime handling, not in the HTML page itself.

This is not a Gate B pass: the map and HTML are not yet proven to compose in
one DreamSeeker window. The next focused step is to trace the X11 reparent,
map, and destroy operations for those WebView2 windows before changing that
driver. The working BYOND prefix and Lutris configuration remain untouched.

Evidence:

- `/tmp/byond-dcomp-childcontent/20260824-201007-hub`
- `/tmp/byond-dcomp-game/20260824-201118-game`
- Wine branch: `dcomp-composition`

### Built-in D3D11/DXGI API probe on the current custom build — 2026-08-27

Because the DreamSeeker mock is not an exact copy of SOA, the next investigation
step was to test the Windows graphics contracts directly. The existing 32-bit
`wine-composition-api` probe was run against the isolated custom Wine build with
Wine's built-in `d3d11`, `dxgi`, `d3d10core`, and `dcomp` DLLs forced. The normal
BYOND prefix and Lutris runner were not changed.

The relevant results were:

```text
D3D11CreateDevice(HARDWARE)                                S_OK
ID3D11Device::CreateTexture2D                              S_OK
IDXGIResource::GetSharedHandle                             E_NOTIMPL
IDXGIResource1::CreateSharedHandle                         E_NOTIMPL
IDXGIKeyedMutex                                             E_NOINTERFACE
IDXGIFactory2::CreateSwapChainForComposition               S_OK
DCompositionCreateDevice                                   S_OK
DCompositionCreateDevice2                                  S_OK
IDCompositionVisual::SetContent                            S_OK
IDCompositionTarget::SetRoot                               S_OK
IDCompositionDevice::Commit                                S_OK
```

This narrows the result without proving that the whole browser will render:
Wine can now create the basic DirectComposition objects, but the D3D11 texture
cannot yet be exported as a shared handle, and the keyed-mutex interface is not
exposed by the D3D11 texture object. Those are the operations needed
when pixels produced by a helper process must be consumed by another process.
The `S_OK` composition calls only show that the object calls return; they do not
show that a browser surface has reached the DreamSeeker window.

The first attempt at this probe was discarded as a comparison because the
launcher did not force Wine's built-in graphics DLLs and therefore used DXVK.
The result above is the authoritative run for this step.

Evidence:

- `/tmp/wine-composition-api-wine-native.log`
- `/home/octavio/Downloads/linux-ports/repro/wine-composition-api`

The same probe was then run with `WINE_D3D_CONFIG=renderer=vulkan`. In that
case the legacy shared-handle path changed from failure to success:

```text
legacy GetSharedHandle       S_OK
legacy OpenSharedResource    S_OK
NT CreateSharedHandle        E_NOTIMPL
keyed mutex                  E_NOINTERFACE
```

This is an important qualification. The current experimental code can export
and reopen a legacy shared texture only through its Vulkan WineD3D path. It is
not a complete implementation and it does not yet provide the NT-handle or
keyed-mutex interfaces. We therefore cannot say that the missing API is fixed
just because one renderer passes the legacy test; the normal DreamSeeker run
must use the same renderer and the browser's actual presentation path must
still be verified.

Evidence: `/tmp/wine-composition-api-vulkan.log`.

### DreamSeeker WebView2 call-path trace — 2026-08-28

The synthetic DreamSeeker run was then traced with Wine's built-in D3D11,
DXGI, and DComp DLLs while forcing WineD3D's Vulkan renderer. This is not a
claim that the synthetic program is identical to SOA. It answers a narrower
question: which Windows graphics calls does WebView2 actually make before the
HTML surface should appear?

The trace shows this sequence:

1. WebView2 creates an `IDXGISwapChain1` with
   `CreateSwapChainForComposition`.
2. It creates a DirectComposition target for a window named
   `Intermediate D3D Window`.
3. It creates a visual, attaches the swap chain with `SetContent`, and calls
   `Commit`.
4. Wine's compositor thread reads the swap-chain buffer and tries to copy it
   into the target window.

The important failure is in the last step. Wine reports:

```text
create_target target window 000100CE class "Intermediate D3D Window"
    parent 000100CA ... visible 0
do_composite composite target 000100CE destination 000100CE
    visible 0 dc 00000000 ... copied 0
```

`dc 00000000` means Wine could not obtain a drawing surface for that window,
and `copied 0` means no browser pixels were copied. This explains why the
reproducer can successfully complete `SetContent` and `Commit` yet show no
HTML panel: those calls only build the composition description; they do not
prove that pixels reached the visible DreamSeeker window.

This also identifies the limitation of the current branch. Its
`CreateSwapChainForComposition`, `IDCompositionVisual::SetContent`, and
`IDCompositionDevice::Commit` implementations are still marked as stubs or
semi-stubs. They use a compatibility copy loop rather than Wine's normal
window compositor. The next source change must therefore address the target
and pixel-presentation path as one design. Adding another WebView2 command
line flag would not repair this failed copy.

Evidence: `/tmp/byond-vulkan-api-trace.log`.

### Hidden-target presentation experiment — 2026-08-28

The compositor was changed in one narrow way: when the DComp target is hidden,
it walks toward a visible ancestor, translates the target position with
`MapWindowPoints`, and copies the swap-chain image at that position. This was
tested in the isolated custom build only.

The first short run ended while the target was being torn down and was not
useful by itself. A longer run showed the normal path clearly:

```text
destination start 00050072 visible 1 parent 000500C0 root 00070060
destination result 00050072 visible 1 offset 0,0
do_composite ... dc 06010055 size 706x582 copied 1
```

`copied 1` proves that Wine successfully copied a browser frame into a live
window device context. It does not prove that the frame is visually correct,
that transparency is preserved, or that several WebView2 visuals are layered
in the right order. The experiment therefore confirms a working pixel-copy
stage, but it is not yet a complete DirectComposition implementation and is
not ready to propose as an upstream patch.

Evidence:

- `/tmp/byond-vulkan-window-lifetime-45s.log`
- `/home/octavio/Downloads/wine-upstream/dlls/dcomp/device.c`

### Step 6 visual-tree slice — 2026-08-28

Step 6 now has a completed, bounded source checkpoint. This is not a claim
that all of DirectComposition is implemented. The change makes the existing
experimental copy bridge understand more of the state that a real browser
composition uses:

- `IDCompositionVisual::SetOffsetX()` and `SetOffsetY()` now retain the
  visual's position and return success.
- `IDCompositionVisual::SetTransform()` now retains the matrix. The copy
  bridge applies its translation components; scaling and rotation are still
  reported as unsupported.
- The compositor walks every content-bearing visual in the tree, in child
  order, instead of finding only the first one. This matters for a HUD made
  from several browser controls, such as a login panel plus inventory panel.
- The reproducer launcher now treats an explicitly empty
  `BYOND_WEBVIEW_ARGS=''` as “use no extra browser flags.” Before this change,
  the shell expansion silently restored the fallback flags, including
  `--disable-d3d11`, so a supposed DirectComposition test could accidentally
  bypass the D3D11 path.

The affected Wine source files are `dlls/dcomp/dcomp_private.h`,
`dlls/dcomp/visual.c`, and `dlls/dcomp/device.c`. The test-launcher correction
is in `repro/dreamseeker-webview2/run.sh`.

The 32-bit DLL compiled successfully and was installed into the isolated
custom Wine installation. The existing D3D11 regression test still reports
the two known TODOs and no unexpected failures:

```text
9 tests executed (2 marked as todo, 0 failures), 0 skipped.
```

The corrected reproducer was then run for 25 seconds with D3D11 enabled and
only `+dcomp,+dxgi,+d3d11` tracing. The trace shows several composition
targets and repeated `copied 1` results. That confirms the new tree traversal
is executing and the internal GDI copy call is succeeding. It still does not
prove correct pixels on the desktop: one hidden ad target continued to copy
to a non-visible destination, and the trace still reports the composition
swap-chain and device commit code as stubs/semi-stubs.

Evidence: `/tmp/byond-step6-tree-d3d11-20260828.log`.

Therefore this checkpoint finishes the visual-tree bookkeeping portion of
step 6, but the overall DirectComposition presentation fix remains open.
The next source work must replace the compatibility GDI copy with a proper
surface-to-window presentation path and then rerun the manual DreamSeeker
gate. Until that happens, the original SOA screenshot/video remains a failed
integration result, not a regression pass.

### Hidden-destination handoff guard — 2026-08-28

The next single source change added a safety check to the same presentation
bridge. If the target and all of its ancestors are hidden, the compositor now
skips that frame instead of obtaining a DC and repeatedly copying pixels into
an invisible window. This is important for the short-lived advertisement
surface, whose renderer remains alive after its window is hidden. A visible
target is still copied normally.

The 32-bit `dcomp.dll` was rebuilt and installed. The same 25-second
D3D11-enabled reproducer trace recorded 172 hidden-destination skips and
continued to record successful copies for visible targets, including the
1258x696 and 1366x696 surfaces. The D3D11 regression test remained at:

```text
9 tests executed (2 marked as todo, 0 failures), 0 skipped.
```

Evidence: `/tmp/byond-dcomp-visible-gate-20260828.log`.

This reduces pointless work and prevents hidden browser frames from entering
the window handoff, but it does not solve the original on-screen composition
failure. The trace still reports `CreateSwapChainForComposition` and
`Commit` as experimental stubs, and successful internal copying still does
not prove correct desktop pixels.

### Early hidden-destination filter correction — 2026-08-29

While reviewing the code, we found that the first version of the guard was
too late: it had already copied the swap-chain image into a temporary D2D
bitmap and acquired a GDI source DC before deciding to skip the hidden
destination. That prevented the final window write, but it was not complete
cleanup and risked leaving the source DC unreleased on the early-exit path.

The guard now runs immediately after the visual's required objects are
checked, before Wine queries the swap chain, allocates the temporary bitmap,
or acquires any DC. A hidden target therefore exits through the ordinary
`done:` cleanup path with no presentation resources acquired. Visible targets
continue through the normal copy path.

The corrected DLL compiled, installed, and passed source whitespace checks.
A 20-second verification run recorded 25 hidden-destination skips and no
visible copies because it ended before the visible game surfaces appeared.
That run verifies the early filter, but it cannot say anything about the
visual result. Evidence: `/tmp/byond-dcomp-visible-guard-early-20260829.log`.

### X11 child-window preservation checkpoint — 2026-08-24

The next trace confirmed a real X11 lifetime problem. Chromium first creates
the browser document as a top-level Win32 window, then reparents it into the
DreamSeeker host. The old X11 driver responded to that transition by
destroying the document's X11 window. The trace showed the document window
being destroyed immediately after `NtUserSetParent`.

The current branch contains a narrow source experiment in
`dlls/winex11.drv/window.c` and `dlls/winex11.drv/x11drv.h`: when an existing
whole X11 window becomes a Win32 child, it is reparented below the host's
top-level X11 window and retained. Child-relative coordinates are preserved.
This is a normal X11 parent/child operation; it is not controlled by a BYOND
environment variable or a browser-specific class-name workaround.

The result proves that part of the diagnosis:

- `/tmp/byond-x11-preserved/20260824-203419-game` contains the test run.
- The log records five `preserving child window` messages and no destruction
  of those five browser document windows.
- The X11 tree places the `3600007`, `3600008`, `3600009`, and `360000a`
  browser windows below DreamSeeker's `2c00006` host window.
- Wine's inventory reports those windows as `IsViewable`.

However, the screenshots still contain only the map; the HTML pixels do not
arrive in the visible child. Therefore X11 destruction/lifetime was a real
bug, but it is not the whole bug. The next source step is to trace which
DirectComposition target receives the WebView2 swap-chain pixels and why that
presentation is not reaching the now-surviving child window. We will not add
another X11 change until that pixel path is understood.

### DirectComposition target and visible-host checkpoint — 2026-08-24

The next trace identified the missing link. WebView2 creates its
DirectComposition target on a child named `Intermediate D3D Window`. That
child is not the visible browser document itself; its parent is the visible
`Chrome_WidgetWin_1` document window. Wine's copy operation returned success,
but it was copying into the invisible intermediate child, so X11 had no
presented browser pixels to show.

The source experiment in `dlls/dcomp/device.c` now walks from an invisible
composition target to its nearest visible ancestor before obtaining the
destination DC. This is still generic: it does not mention BYOND, WebView2,
or a window class. The trace also added temporary information about the
target's class and the copy result while this behavior was being verified.

The first normal DreamSeeker run with this handoff reached the real login
form after the advertisement:

- `/tmp/byond-dcomp-parent-normal/20260824-204935-game/screens/game-0005.png`
  shows the map and HTML form in one window, without the former full-window
  white surface.
- `/tmp/byond-dcomp-parent-normal/20260824-204935-game` shows that the
  process stayed alive through the 75-second run.
- `/tmp/byond-dcomp-parent-hub/20260824-205300-hub/screens/active-0006.png`
  shows the Hub still rendering its normal interface.

This is a real partial pass for the pixel path, not a final fix. The form's
background is currently too transparent: the map is visible through the dark
login panel, unlike the Windows reference image. Hover stability and the
form's removal after login still need manual validation. The next focused
source question is the swap-chain alpha mode: Wine's DXGI path currently
discards `DXGI_ALPHA_MODE_PREMULTIPLIED`/`DXGI_ALPHA_MODE_IGNORE` metadata,
which can explain the remaining background mismatch. No new X11 change is
planned until that alpha behavior is understood.

The first alpha-aware paint attempt was then tested and rejected. WebView2's
trace requests `DXGI_ALPHA_MODE_PREMULTIPLIED` (`0x1`) for the 1258x740 HTML
surface and `DXGI_ALPHA_MODE_IGNORE` (`0x3`) for the 1366x740 surface. Making
the latter opaque caused the full-window white result to return, even though
the centered form was present. The branch therefore keeps the swap-chain
alpha metadata available through `GetDesc1`, but does not treat `IGNORE` as a
command to paint an opaque full-window surface. Correct DirectComposition
surface ordering and background semantics remain the next source problem.

Evidence for the rejected alpha paint:

- `/tmp/byond-alpha-preserved/20260824-210414-game/screens/game-0005.png`
- `/tmp/byond-dxgi-alpha/20260824-205727-game/wine-console.log`

## First-branch baseline — 2026-08-24

The Wine checkout used for source work is:

```text
/home/octavio/Downloads/wine-upstream
branch: codex/d3d11-shared-resources
commit: e99fc2f7
source tree: clean upstream
```

The existing 32-bit build directory was reconfigured with Wine tests enabled;
the BYOND prefix and the working Lutris runner were not changed. The complete
`d3d11` test executable was also attempted, but its many unrelated OpenGL and
driver-dependent tests produce noise in this machine's graphics environment.
That result is not being used as the pass/fail signal for this investigation.

The focused baseline was run with native DXVK explicitly disabled so that the
test exercised the clean build's WineD3D path:

```bash
cd /home/octavio/Downloads/linux-ports
WINEDLLOVERRIDES='d3d11=builtin;dxgi=builtin;d3d10core=builtin' \
WINEDEBUG=-all \
BYOND_PREFIX=/home/octavio/Downloads/linux-ports/byond32-upstream-baseline \
BYOND_RUNNER=/home/octavio/Downloads/wine-build-upstream32/install \
./repro/wine-composition-api/run.sh
```

The clean upstream build can create the D3D11 device and the textures, but the
resource handoff stops here:

```text
D3D11CreateDevice(HARDWARE)                                S_OK
ID3D11Device::CreateTexture2D                              S_OK
IDXGIResource::GetSharedHandle                             E_NOTIMPL
IDXGIResource1::CreateSharedHandle                         E_NOTIMPL
IDXGIKeyedMutex                                             E_NOINTERFACE
IDXGIFactory2::CreateSwapChainForComposition               E_NOTIMPL
DCompositionCreateDevice                                   E_NOTIMPL
DCompositionCreateDevice2                                  E_NOTIMPL
```

This probe is a single-process API baseline. It does **not** yet prove that a
resource can cross a process boundary; that is the first Wine-test change on
this branch. The next test will have a parent process create a shared texture,
pass its handle to a child process, and have the child call the matching
`OpenSharedResource` method. The test will be marked as an expected upstream
failure until the implementation exists, so adding it will not hide unrelated
graphics-test failures.

The source inspection explains why this test comes first. The D3D11 and DXGI
wrappers currently return `E_NOTIMPL` for the shared-handle/open operations,
while the underlying WineD3D resource has no cross-process sharing identity or
export/import operation. Wine's lower-level `win32u` D3DKMT code already has
generic server-backed resource objects, but the D3D11/DXGI resource wrappers
do not use that machinery yet. We therefore need a failing two-process test
before deciding exactly how much of that path to connect.

### First two-process test result — 2026-08-24

The first Wine-side test is now in
`dlls/d3d11/tests/d3d11.c` on `codex/d3d11-shared-resources`. Its parent mode
creates a 64x64 keyed-mutex texture, requests both kinds of Windows shared
handle, and—when a handle is returned—starts a child instance of the same test
binary. The legacy child calls `ID3D11Device::OpenSharedResource`; the NT-handle
child calls `ID3D11Device1::OpenSharedResource1`. The child also checks that the
keyed-mutex interface survives the handoff.

The focused command was:

```bash
WINEPREFIX=/home/octavio/Downloads/linux-ports/byond32-upstream-baseline \
WINEDLLOVERRIDES='d3d11=builtin;dxgi=builtin;d3d10core=builtin' \
WINEDEBUG=-all \
/home/octavio/Downloads/wine-build-upstream32/install/bin/wine \
  /home/octavio/Downloads/wine-build-upstream32/dlls/d3d11/tests/i386-windows/d3d11_test.exe \
  d3d11 test_shared_resource_process
```

Clean upstream produced:

```text
legacy shared handle: Failed to create ... hr 0x80004001 (E_NOTIMPL)
nt shared handle:     Failed to create ... hr 0x80004001 (E_NOTIMPL)
9 tests executed (2 marked as todo, 0 failures)
```

This is the correct baseline: it confirms the test reaches the missing
shared-handle APIs without introducing a failing Wine test. The child process
cannot be reached until at least one creation method exists, so this result is
not yet a successful cross-process handoff. The next single code change must
make one generic handle path work, after which this same test will expose any
failure in inheritance, resource reopening, or keyed-mutex transfer.

## Current technical hypothesis

There are two related but not yet proven Wine defects that may be involved:

### 1. WebView2 presentation / DirectComposition and shared-surface path

Chromium/WebView2 can render in a helper process, but its final surface must cross process boundaries and be presented through Windows composition APIs. Wine’s implementation may not provide the shared texture, synchronization, or DirectComposition behavior expected by WebView2.

Wine bug 59370 describes blank WebView2 windows, identifies the D3D11 shared-texture/DirectComposition path, and lists missing cross-process texture handoff as a likely cause:

<https://list.winehq.org/hyperkitty/list/wine-bugs%40list.winehq.org/thread/UDZKZTMP5WYISOAUDGEOMFOSU7X73JND/>

That report was closed as a duplicate of bug 58921. “Duplicate” means the report was merged into another report; it does **not** mean the bug was fixed.

### 2. X11 child-window lifetime/ordering (real defect, but not sufficient)

DreamSeeker contains a Direct3D map window and WebView2-managed native host windows. Wine traces show repeated `invalidate_dce` activity involving those HWNDs, but the detailed X11 trace changed the interpretation. The 1016x509 `Chrome_WidgetWin_0` host is created without `WS_VISIBLE` and remains intentionally unmapped; Chromium uses it as an internal composition host rather than as a normal independently displayed X11 window. No ordinary `ShowWindow`/`XMapRaised` omission has been found.

The later X11 trace found a real lifetime problem in the ordinary embedded-window path: when WebView2 changes a browser HWND from a desktop child to a DreamSeeker child, Wine destroys the browser HWND's old X11 whole window. That is a legitimate Wine bug candidate, but preserving that X11 window did not make the HTML form appear. The `invalidate_dce` loop is therefore evidence that the two rendering systems interact, not proof that stacking alone is the root cause. A complete fix must preserve the child window where appropriate **and** repair the browser pixel handoff/composition path.

## Current upstream source inspection

The current upstream checkout is Wine development version `11.15`, commit:

```text
e99fc2f
mshtml: Don't leak the attribute after forwarding in setAttributeNode().
```

The unmodified source still contains relevant unimplemented paths:

- `dlls/dcomp/device.c`: `DCompositionCreateDevice`, `DCompositionCreateDevice2`, and `DCompositionCreateDevice3` return `E_NOTIMPL`.
- `dlls/dxgi/resource.c`: `IDXGIResource1::CreateSharedHandle` returns `E_NOTIMPL`.
- `dlls/d3d11/device.c`: `ID3D11Device::OpenSharedResource` and `ID3D11Device1::OpenSharedResource1` return `E_NOTIMPL`.

The current `dlls/dcomp` directory contains only `Makefile.in`, `dcomp.spec`, and `device.c`; it does not contain an implemented DirectComposition device/visual/surface object model. This makes DirectComposition relevant, but not automatically sufficient: Wine-Staging already carries a substantial experimental DComp patchset, while the WebView2 109 failure can still occur because the cross-process shared-surface path is incomplete.

This does not prove that implementing all of these functions will fix BYOND. The Wine bug analysis says Chromium's ANGLE/SharedImage path may bypass these ordinary APIs. It does prove that the current upstream tree does not yet have a complete implementation of the Windows interfaces involved in this family of WebView2 composition failures, so a source-level investigation is justified.

## Upstream status checked

Bug 58921 reports that WebView2 works with Windows 7/8 settings but fails with newer settings because it attempts DirectComposition behavior that Wine does not implement completely:

<https://list.winehq.org/hyperkitty/list/wine-bugs%40list.winehq.org/2026/3/?count=50&page=4>

Bug 59370 was closed as a duplicate rather than marked fixed:

<https://list.winehq.org/hyperkitty/list/wine-bugs%40list.winehq.org/thread/UDZKZTMP5WYISOAUDGEOMFOSU7X73JND/>

Wine 11.14 contains some unrelated or narrower WebView2/window fixes, but its release announcement does not list bugs 58921 or 59370 as fixed:

<https://list.winehq.org/hyperkitty/list/wine-announce%40list.winehq.org/2026/7/>

Some related problems have been fixed in Wine. For example, the WebView2 sandbox/freeze report 56378 was closed as fixed in Wine 11.1, and a separate black foreground rectangle report 59378 is listed in the Wine 11.14 fixes. Those fixes show that Wine’s WebView2 compatibility is improving, but they do not establish that the shared-surface and DreamSeeker flicker problem is fixed.

## Standalone composition probe: 2026-08-13

`webview-composition-probe.c` was added to test the Windows interfaces independently of BYOND. It creates a D3D11 device, queries its DXGI device, creates a shared texture, and asks for shared-handle operations that a browser compositor may need. It does not modify the working BYOND prefix.

Against the current Wine-Staging 11.14 runner, the probe produced:

```text
DCompositionCreateDevice(NULL): hr=0x00000000
D3D11CreateDevice: hr=0x00000000
ID3D11Device::QueryInterface(IID_IDXGIDevice): hr=0x00000000
shared texture creation: succeeds
IDXGIResource::GetSharedHandle: hr=0x80004001 (E_NOTIMPL)
IDXGIResource1::CreateSharedHandle: hr=0x80004001 (E_NOTIMPL)
```

This separates two ideas that are easy to confuse:

- DirectComposition device creation exists in the staging runner.
- The shared texture handle operations are still unimplemented there.

The current upstream 11.15 build also returns `E_NOTIMPL` for DirectComposition creation, and its probe could not create a D3D11 device because that isolated build was initially configured without the 32-bit OpenGL development linker files. That makes the first upstream probe an incomplete graphics comparison, but it does not change the DreamSeeker result: the page/controller are healthy and the form is still absent.

The official Wine bug analysis is consistent with this probe. It describes WebView2 109 as a multi-process pipeline in which ANGLE/Chromium renders into shared surfaces and a compositor must transfer those pixels into the application. A successful DOM or D3D device call therefore does not guarantee that any pixels reach the final window. See [Wine bug 59370](https://list.winehq.org/hyperkitty/list/wine-bugs%40list.winehq.org/thread/UDZKZTMP5WYISOAUDGEOMFOSU7X73JND/).

### DreamSeeker API trace comparison

Run: `test-results/20260813-141112-game`

The 68-second run used the persistent software/fallback arguments and enabled `+d3d11,+dxgi,+dcomp,+d3d9`. It reached the game after the 50-second advertisement, created the login DOM, and still showed no form. The Wine console contained many normal D3D9 `Present` calls, but zero calls to `CreateSharedHandle`, `GetSharedHandle`, `OpenSharedResource`, or DirectComposition entry points.

Run: `test-results/20260813-141313-game`

The same test with `BYOND_WEBVIEW_ARGS=` (no extra browser flags) also reached the game and created the login DOM, but the form was still absent. EGL initialization then reported that the available GLES implementation could not provide GLES3. This rules out the simple explanation that the persistent workaround flags alone caused the missing overlay.

Together with Wine bug 59370, this means the first custom patch cannot honestly be “implement the three shared-handle stubs and the problem is solved.” Those APIs are important generic Wine work, but this WebView2 109 path may use Chromium's internal SharedImage/mailbox handoff without calling them. We need to trace or reproduce that handoff before changing rendering code.

## Custom experiment A: force an off-screen Chrome host surface

Source change: `wine-upstream/dlls/win32u/window.c`

The experiment added an opt-in `WINE_FORCE_HIDDEN_SURFACES=1` path. When enabled, Wine allocates a driver window surface for hidden top-level windows whose class begins with `Chrome_WidgetWin_`. The normal behavior is unchanged when the variable is absent. This was intentionally restricted to the disposable test build; the executable-name/class-name condition is not suitable for an upstream patch.

The patched `win32u.so` was rebuilt and copied only into the isolated Wine installation:

```text
/home/octavio/Downloads/wine-build-upstream32/install/lib/wine/i386-unix/win32u.so
```

Run: `test-results/20260813-142246-game`

- Wine: isolated upstream 11.15 plus the opt-in win32u experiment
- `WINE_FORCE_HIDDEN_SURFACES=1`
- Map: nonblank
- Advertisement transition: Continue succeeded at 50 seconds
- Login DOM: `YES`
- Controller failure: none
- Login form: still not composited

Result: **negative**. Allocating a normal off-screen X11 driver surface for the hidden Chrome host is not enough. The missing pixels occur later in the browser-to-game composition path. The experiment remains in the source tree as a documented diagnostic, but it is not a proposed fix and is not enabled by the normal runner.

## Browser flag experiment: explicit DirectComposition disable

Run: `test-results/20260813-142557-game`

The warmed Wine-Staging 11.14 test added `--disable-direct-composition` to the existing fallback arguments. It still reached the map, automatically crossed the advertisement at about 50 seconds, created the login DOM, and reported no WebView2 controller failure. The HTML form remained absent; the WebView host remained unmapped in all 15 captures. One transient blank game frame was recorded during the transition.

Result: **negative**. Explicitly disabling DirectComposition does not select a working visible-window fallback for this BYOND/WebView2 combination.

## Win32 owner/parent probe during the login-form stage

Run: `test-results/20260813-143030-game`

At about one minute into a fresh DreamSeeker run, after the automatic advertisement transition, the diagnostic Win32 probe reported the actual WebView child hierarchy:

```text
scope=child hwnd=000105e6 pid=32 visible=1 parent=000105c6 owner=00000000 style=56010000 exstyle=00000000 class=Chrome_WidgetWin_0 title= rect=54,28,1312,724
scope=child hwnd=000105f0 pid=360 visible=1 parent=000105e6 owner=00000000 style=56000000 exstyle=00000000 class=Chrome_WidgetWin_1 title=127.0.0.1:39045/e33cfbfa.htm rect=54,28,1312,724
scope=child hwnd=00020612 pid=360 visible=1 parent=000105f0 owner=00000000 style=56300000 exstyle=00000020 class=Chrome_RenderWidgetHostHWND title=Chrome Legacy Window rect=54,28,1312,724
```

The parent links are correct: DreamSeeker game -> `Chrome_WidgetWin_0` -> `Chrome_WidgetWin_1` -> `Chrome_RenderWidgetHostHWND`. The earlier top-level-only probe was misleading because it omitted child windows; it must not be used as evidence that WebView2 lost its Win32 parent.

The X11 trace then showed what happens during the transition into that child hierarchy:

```text
x11drv:X11DRV_SetParent SetParent hwnd 0x105e4 parent 0x105dc old_parent 0x10020 style 46000000 ... current_xwin 3400009 parent_xwin 0
x11drv:destroy_whole_window win 0x105e4 xwin 3400009/0
```

Here `0x3400009` is the real login-document X11 window, not the unrelated hidden 1016x509 helper. The `SetParent` operation moves the Win32 window from the desktop into the game, but Wine's X11 driver destroys the old X11 whole window because the new Win32 parent has no separate whole window.

An earlier `--raise` probe targeted the separate 1016x509 hidden helper and produced a black capture. That result is useful only for the helper-window path; it must not be presented as a capture of the actual login document. The controlled child-window tests distinguish two layers of failure:

1. The browser's internal top-level helper is intentionally unmapped, while the real login document is a visible Win32 child.
2. Wine destroys the real login document's X11 whole window during reparenting. Keeping that X11 window alive makes it viewable and correctly nested, but still does not reveal the HTML form.

This led to an opt-in diagnostic change in `winex11.drv`, enabled only with `WINE_KEEP_CHILD_WINDOWS=1`. It keeps the native X11 window and reparents it under the DreamSeeker top-level instead of destroying it. The resulting X11 tree is now structurally correct:

```text
0x2c00006 "Shinobi Online Adventure...": 1366x696+0+0 +0+28
   0x3400009 "127.0.0.1:43901/e33cfbfa.htm": 1258x696+0+0 +0+28
```

However, the result is still negative:

- `test-results/20260813-151323-game`: software/fallback browser flags, child window kept, form absent.
- `test-results/20260813-151618-game`: same experiment with the current software flags, form absent.
- `test-results/20260813-151823-game`: native browser graphics, child window kept, form absent.
- `test-results/20260813-152022-game`: native graphics plus direct embedded-window captures; the real 1258x696 login window is viewable, but `screens/embedded-0023.png` contains the map/title image and no login form.

The 281x281 `Sign In - Backup` screenshots from `test-results/20260813-144736-game` are DreamSeeker's own fallback UI. They are not WebView2 pixels and must not be counted as proof that WebView2 rendered the form.

The conclusion is precise: the X11 child lifetime/reparenting problem is real and worth isolating for a future Wine patch, but fixing it alone does not solve the BYOND form. The remaining failure is inside the browser-to-host presentation path (Chromium/ANGLE/SharedImage/DirectComposition or its Wine equivalent), where the actual HTML pixels are not reaching the DreamSeeker composition surface.

The `WINE_KEEP_CHILD_WINDOWS` change remains a diagnostic experiment, not an upstream-ready patch. It is broad, changes window lifetime semantics for arbitrary child windows, and still fails the rendering test.

## Decisive Chromium-versus-Wine comparison: 2026-08-13

Run: `test-results/20260813-153226-game`

The test runner was given Chromium's local DevTools port, and the new helper `/home/octavio/Downloads/linux-ports/webview-cdp-capture.py` requested a screenshot directly from the WebView2 page:

```text
--remote-debugging-port=9230
```

The resulting image is:

`test-results/20260813-153226-game/cdp-login.png`

It is a 1258x696 PNG containing the complete login form: Sign In/Register tabs, Username, Password, Remember me, LOG IN, and the recovery links. In the same run:

- DreamSeeker map: 14 nonblank frames, no blank/white frames.
- Login DOM: `YES`.
- WebView2 controller failure: none.
- Wine/X11 WebView host: 22 detected captures, 0 viewable captures for the separate host window.
- DreamSeeker game captures: map only; the form was not visible.

This is the clearest proof so far. In simple terms, Chromium has already painted the form correctly on its own canvas. Wine then fails while moving that finished picture into the embedded DreamSeeker window. The form is not broken, and the page is not blank. The missing step is the bridge between Chromium's compositor and DreamSeeker's Direct3D surface.

The `embedded-*.png` files from the child-window experiment must be interpreted carefully: because the native X11 child is not a normal independently composited browser surface, capturing that X11 window can show the underlying map. The DevTools screenshot is the reliable measurement of Chromium's own output; the game screenshot is the reliable measurement of Wine's final output.

This result narrows the upstream work substantially. A fix should focus on the presentation bridge—Chromium/ANGLE SharedImage handoff, cross-process texture synchronization, DirectComposition, or Wine's D3D9/D3D11 interoperation—not on HTML, BYOND server behavior, or ordinary X11 focus/raise operations.

## Historical source-level compositor experiment: 2026-08-13

At the time of this historical experiment, the disposable upstream tree was
based on Wine development commit `f3d2cbcff79ac0dc0e212d90484755a9340964ba`
and was built as a 32-bit runner at:

```text
/home/octavio/Downloads/wine-build-upstream32/install
```

The working Lutris runner and `/home/octavio/Games/byond32` prefix were not changed.

The experiment tested three separate Wine-side mechanisms:

1. Keep a native X11 window alive when a foreign-process WebView child changes from a desktop window to a DreamSeeker child.
2. Give the foreign child its own X11 drawable and ARGB visual instead of accidentally drawing it through the DreamSeeker parent drawable.
3. Use XRender to copy a foreign alpha surface after the DreamSeeker OpenGL present.

The first two are real defects or credible defects in Wine's X11 child-window path. The third is useful as a diagnostic, but is not yet an upstream patch: WebView2 sometimes exposes a full-size browser container whose pixels are opaque black/white under the software fallback path. Copying that surface can therefore create a full-page white rectangle. A later run with the normal nested child hierarchy showed the HTML surface is present and viewable, but the actual desktop still showed the map without the form.

The strongest controlled result is therefore:

```text
Hub test, patched isolated Wine:       PASS (12 nonblank frames, no controller error)
DreamSeeker map:                       PASS (nonblank after advertisement)
DreamSeeker login DOM:                 YES
WebView2 controller failure:            NO
HTML form on final desktop composite:  NOT FIXED
```

The test command is:

```bash
cd /home/octavio/Downloads/linux-ports
env \
  BYOND_PREFIX=/home/octavio/Downloads/linux-ports/byond32-upstream-baseline \
  BYOND_RUNNER=/home/octavio/Downloads/wine-build-upstream32/install \
  BYOND_TEST_RESULTS=/home/octavio/Downloads/linux-ports/test-results \
  BYOND_AUTO_CONTINUE=1 \
  WINE_KEEP_CHILD_WINDOWS=1 \
  WINE_FORCE_HIDDEN_SURFACES=1 \
  WINE_WEBVIEW_ARGB=1 \
  WINE_FOREIGN_CHILD_SURFACES=leaf \
  ./tools/byond-test-runner.sh --game byond://198.166.109.89:3336 --duration 72 --interval 2
```

The relevant log line proves that the experimental compositor is reached after the game presents:

```text
composited foreign child ... into 3000006 ...
```

That rules out “the patch was never called.” It also demonstrates why this cannot yet be proposed as the final fix: X11 composition is receiving the browser surface, but the surface's alpha/ownership semantics are not consistently correct.

## PR plan

Do not open an upstream Wine PR for the current exploratory diff. It contains test-only environment switches, Chrome class-name checks, root-overlay behavior, and an X11 copy path that can make arbitrary applications opaque. Those are laboratory tools, not a safe general fix.

The reviewable upstream work follows the branch plan above:

1. A generic D3D11/DXGI shared-resource change, with a test that passes a
   resource between two Wine processes. The existing Wine-Staging DComp work
   still leaves `IDXGIResource1::CreateSharedHandle` and
   `ID3D11Device::OpenSharedResource` incomplete, so this is the first branch.
2. A separate DirectComposition presentation change, with tests for the
   device, visual/content, swap-chain, and commit behavior needed to present
   the shared resource. It will normally be reviewed on top of the first
   branch, but it must remain a separate logical change.
3. A generic X11 foreign-child lifetime/drawable change only if a focused test
   proves that the child window is still lost after the graphics path works.
   This branch must not mention BYOND or WebView2 and is independent of the
   first two branches.

Until the second test passes, the honest status is “diagnosed and instrumented, not fixed.” The source tree and results remain available so the next change can be tested against the same 50-second advertisement transition instead of relying on a screenshot taken too early.

An attempted repeat using the original Wine 11.14 Staging runner (`test-results/20260813-153527-game`) was stopped because the copied prefix entered Wine Mono's first-run installer before launching DreamSeeker. Its zero game frames are setup failure, not a rendering result. The earlier completed Staging runs remain the valid control results.

Hub regression check: `test-results/20260813-152658-hub`

- Wine: isolated upstream 11.15 plus the child-window experiment
- Hub: 5 nonblank frames, 4 unique hashes
- Controller failure: none
- Automated surface result: `PASS`

The Hub did not regress under the experiment. This is expected because its WebView is top-level rather than being reparented into the DreamSeeker child hierarchy, but it is still useful evidence that the diagnostic change did not break the working Hub surface.

## Decision: build current Wine once, then patch the same tree

We will not spend time trying many unrelated Lutris runners. The efficient path is:

1. Keep Wine 11.14 and `/home/octavio/Games/byond32` as the untouched control.
2. Build current upstream Wine from source in an isolated build/install directory.
3. Run the unmodified upstream build against a separate copy of the BYOND prefix, using exactly the same flags and test durations.
4. If current upstream works, record the version and identify the upstream change.
5. If it fails, use the same source checkout for the first custom patch.
6. Re-run the Hub and DreamSeeker tests after each patch.
7. Use the real DreamSeeker run as the integration test. Any future Wine
   regression test must be based on a deterministic, portable Wine behavior,
   not on the discarded harness.

This gives us both the “newer Wine” control and a clean path to a custom fix without rebuilding the environment twice.

## Required test protocol

Every run must record:

- Wine build/version and source commit
- prefix path and architecture
- Windows-version overrides
- exact WebView2 arguments
- DXVK/WineD3D choice
- display backend
- WebView2 runtime version
- Hub duration and result
- DreamSeeker duration and result
- whether the login DOM was created
- whether the WebView window was `IsUnMapped` or `IsViewable`
- screenshots taken during the 40–50 second advertisement transition and after the login form appears

The current runner and analyzer are:

- `/home/octavio/Downloads/linux-ports/tools/byond-test-runner.sh`
- The former analyzer was removed when the experiments were consolidated.

The game test must last at least 95 seconds so the advertisement transition is not accidentally skipped.

## What would qualify as an upstreamable fix

Before opening a Wine merge request, the patch should:

- reproduce the behavior without relying only on BYOND;
- identify the affected Wine subsystem (`dcomp`, `d3d11`, `dxgi`, `win32u`, or X11 driver);
- avoid hard-coded BYOND executable names or game coordinates;
- preserve normal behavior for ordinary Win32 child windows;
- include a regression test where practical;
- show before/after results on the real DreamSeeker and Hub runs;
- be tested with both the working Wine 11.14 baseline and current upstream.

Wine contribution information is available at <https://www.winehq.org/getinvolved>.

## Build preparation history

The current Wine source checkout and an isolated 32-bit build directory have been prepared:

- Source: `/home/octavio/Downloads/wine-upstream`
- Build directory: `/home/octavio/Downloads/wine-build-upstream32`

The initial configuration found missing 32-bit development libraries. They were installed manually because the desktop tool cannot enter the local `sudo` password:

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc-multilib g++-multilib libc6-dev-i386
sudo apt-get build-dep -y wine
```

### Build checkpoint: 2026-08-12

After the multilib dependencies were installed, the Linux 32-bit compiler test passed. Wine configuration then stopped at the next required component:

```text
configure: error: i386 PE compiler not found.
Please install mingw-w64, clang/llvm-dlltool/lld, or llvm-mingw.
```

The Debian package needed for this build is:

```bash
sudo apt-get install -y gcc-mingw-w64-i686
```

The x86-64 MinGW compiler was installed by the generic Wine build dependencies, but the i686 MinGW compiler was not.

The i686 MinGW compiler was then installed successfully. The next configure check stopped because the 32-bit X11 development packages were absent. The installed 64-bit `-dev` packages are not sufficient for Wine's 32-bit build.

The missing X11 package set was:

```bash
sudo apt-get install -y \
  libx11-dev:i386 libxext-dev:i386 libxrender-dev:i386 \
  libxfixes-dev:i386 libxrandr-dev:i386 libxinerama-dev:i386 \
  libxcursor-dev:i386 libxi-dev:i386 libxcomposite-dev:i386 \
  libxdamage-dev:i386 libxxf86vm-dev:i386 libxkbfile-dev:i386 \
  libxkbregistry-dev:i386 libxss-dev:i386 libxv-dev:i386 \
  libxshmfence-dev:i386 libxkbcommon-dev:i386
```

After those X11 development packages were installed, Wine configure detected 32-bit X11 successfully. It then stopped because 32-bit FreeType development files were missing:

```text
configure: error: FreeType 32-bit development files not found.
```

The missing FreeType package was:

```bash
sudo apt-get install -y libfreetype-dev:i386
```

## Build and unmodified upstream baseline: 2026-08-13

The missing FreeType package and the previously requested i686 MinGW/X11 packages were installed. The source was configured as a 32-bit-only build with tests disabled:

```bash
../wine-upstream/configure --prefix="$PWD/install" --disable-tests
```

The desktop sandbox refuses to execute 32-bit Linux helper programs and returned `Bad system call` for Wine's generated `makedep`, `winebuild`, `widl`, `wrc`, `winegcc`, `make_xftmpl`, `wmc`, `sfnt2fon`, and `install` tools. These were rebuilt as host-side 64-bit build helpers only; the Wine runtime and all Windows DLLs remained 32-bit. This workaround changes the build environment, not Wine behavior.

The unmodified upstream source then built and installed successfully:

- Source commit: `f3d2cbcff79ac0dc0e212d90484755a9340964ba`
- Wine version: `wine-11.15` (development)
- Source: `/home/octavio/Downloads/wine-upstream`
- Build/install: `/home/octavio/Downloads/wine-build-upstream32`
- Runtime architecture: 32-bit i386
- WebView2 runtime: `109.0.1518.140`
- Test prefix copy: `/home/octavio/Downloads/linux-ports/byond32-upstream-baseline`
- Working prefix `/home/octavio/Games/byond32`: untouched

Wine Mono 8.1.0 was copied from an existing local Lutris runner into the isolated upstream installation because the first baseline launch stopped at the first-run Wine Mono installer. The working BYOND prefix was not changed.

### Hub baseline result

Run: `test-results/20260813-133838-hub`

- Hub launched and remained running for the 35-second test.
- Hub surface: 8 nonblank frames, 7 unique hashes; automated result `PASS`.
- WebView2 controller failure: none.
- WebView2 process: launched successfully.
- The separately detected WebView2 window was still reported `IsUnMapped` in 10 captures, despite the Hub surface being visible.

### DreamSeeker baseline result

Run: `test-results/20260813-133935-game`

- DreamSeeker map: 28 nonblank frames, 3 unique hashes; automated result `PASS`.
- Advertisement transition: automatic Continue click succeeded at elapsed 50 seconds.
- Login DOM: `YES`.
- WebView2 controller failure: none.
- WebView2 window: 57 unmapped captures and 0 viewable captures.
- Visual result: the map and title screen are visible, but the HTML login form is not composited over the map. Active-window captures are black when the unmapped WebView surface receives focus.

This is the decisive control result: moving from Wine 11.14 staging to current unmodified upstream Wine 11.15 does not fix the DreamSeeker HTML overlay. The issue remains a Wine presentation/child-window composition problem, not a broken BYOND page, broken WebView2 runtime, or failed login DOM. Because the browser host is intentionally hidden, the `IsUnMapped` count is diagnostic context rather than the fix target.

## Current build state after baseline: 2026-08-13

The required compiler, MinGW i686, X11 i386, and FreeType i386 dependencies were installed. The isolated build can produce a 32-bit `wine-11.15` runtime. The desktop sandbox cannot execute 32-bit Linux helper programs, so a few disposable build helpers had to be rebuilt as host-side 64-bit tools; the Windows DLLs and Wine runtime remain 32-bit.

The first configure did not detect 32-bit OpenGL because the unversioned i386 linker name `libGL.so` was absent even though the runtime `libGL.so.1` existed. Network/DNS was unavailable for downloading the missing Debian development package. A temporary, isolated linker directory was therefore used:

```bash
mkdir -p /tmp/byond-wine-libgl-i386
ln -sfn /usr/lib/i386-linux-gnu/libGL.so.1 /tmp/byond-wine-libgl-i386/libGL.so
LDFLAGS='-L/tmp/byond-wine-libgl-i386' \
  ../wine-upstream/configure --prefix="$PWD/install" --disable-tests
```

Configure then detected 32-bit OpenGL. A narrow rebuild of the graphics DLLs was started but interrupted because the dependency graph expanded into a large rebuild, including vkd3d. The installed `d3d11`/`wined3d` DLL timestamps have not been treated as proof of a completed OpenGL-enabled rebuild. One opt-in `win32u` diagnostic experiment has been applied and tested, but it was negative; no claim is made that the current installed build is a valid accelerated comparison.

The latest source experiments have now tested both sides of the boundary. A generic `winex11.drv` child-window attachment change and the opt-in `WINE_KEEP_CHILD_WINDOWS=1` lifetime workaround can preserve the native browser window, but neither produces the HTML pixels. Native browser graphics and the software fallback both fail in the same isolated prefix. The next source task is to capture Chromium's own output (for example through its DevTools protocol) during the real DreamSeeker run and compare it with Wine's final surface. Only after that evidence should we implement D3D11/SharedImage/DirectComposition support or submit an X11 child-window patch.

## Current status update: 2026-08-13 21:00

The isolated Wine 11.15 build and repeatable 50-second test flow are now operational. The most recent controls are:

```text
Patched isolated Wine + BYOND Hub:        PASS
Patched isolated Wine + DreamSeeker map:  PASS
WebView2 login DOM:                       YES
WebView2 controller RPC failure:          NO
Final map-plus-HTML desktop composite:    NOT FIXED
```

The X11 experiment proved that Wine reaches the presentation hook and can assemble a synthetic map-plus-form image from the two surfaces. It did not prove that WebView2's real alpha/shared-surface contract is being preserved: the browser surface can still arrive as an opaque full-page black/white layer. Therefore the current Wine source diff is retained as an instrumented lab branch, not described as a finished fix.

The next implementation target is the alpha-preserving DirectComposition/shared-surface path. A PR should contain a portable Wine regression test if the failing contract can be isolated, followed by the implementation and the BYOND regression run. No upstream PR has been opened yet.

## EGL finding and next rebuild: 2026-08-13 21:40

The latest tests separated three paths:

```text
Wine 11.15 custom + software WebView2:   HTML DOM YES, form surface YES, final overlay NO
Wine 11.15 custom + native GPU:          HTML DOM YES, controller error NO, form surface black
Wine-Staging dcomp patch + native GPU:    same result; no dcomp calls were logged
```

The native Edge 109 log reports:

```text
eglCreateContext: Requested GLES version (3.0) is greater than max supported (2, 0)
Failed to create GLES3 context, fallback to GLES2
```

At the same time, the custom Wine runtime prints:

```text
warn:wgl:egl_init EGL support not compiled in!
```

The 32-bit Wine configure log confirms that `libEGL` detection failed because the i386 EGL development package was missing. The next required host action is:

```bash
sudo apt-get install -y libegl1-mesa-dev:i386
```

This is now the highest-value next test. The 32-bit `winex11.drv` must be rebuilt after installation, then the same native WebView2 test must be repeated. If the browser produces a valid alpha surface after that rebuild, the practical fix is a real EGL-capable Wine build rather than an X11 stacking hack.

The raw X11 capture from `test-results/20260813-213734-game/live-root.ppm` also showed that the software WebView document surface was a depth-32 ARGB window but its sampled alpha was `0xff` at both the corner and card center. In other words, the X11 driver had an alpha-capable visual, but the browser output arriving in it was already opaque. This rules out simple child-window z-order as the complete solution.

## EGL rebuild and layered-surface experiment: 2026-08-13 23:05

The requested package was installed and the isolated 32-bit Wine build was rebuilt:

```bash
sudo apt-get install -y libegl1-mesa-dev:i386
```

The new runtime now logs `Using EGL OpenGL backend` and no longer reports `EGL support not compiled in!`. This is a valid improvement to the test runner, but it did not fix WebView2. Edge 109 still reports that the requested GLES 3.0 context is above Wine's exposed GLES 2.0 maximum, and the native-GPU run remains black. The relevant run is `test-results/20260813-225122-game`:

```text
map frames:                 5 nonblank
login DOM:                  YES
WebView2 controller error:  NO
viewable browser frames:   1
HTML overlay:               NOT FIXED
```

An additional opt-in experiment forced embedded `Chrome_WidgetWin_*` windows through Wine's layered-surface path and taught the X11 surface copier not to force the alpha byte to `0xff`. Both runs were negative (`20260813-225946-game` and `20260813-230157-game`): the browser windows became unmapped, the DreamSeeker map was not captured, and the login DOM was not reached. The reason is now clear from the trace: the WebView window belongs to another Wine process, so `win32u` cannot create a normal local Wine window surface for it. This experiment is not an upstream candidate and must not be enabled in the working configuration.

The current evidence therefore splits the problem into two independent Wine gaps:

1. Native Edge rendering needs a usable GLES 3/ANGLE path under 32-bit Wine.
2. Software or fallback rendering needs a cross-process WebView surface contract that preserves transparency instead of presenting an opaque X11 child.

The first gap is not fixed merely by compiling Wine with EGL. The second is not fixed merely by changing X11 stacking or forcing a local layered window. The next useful source change must target the cross-process browser surface itself, with pixel/alpha evidence from the real DreamSeeker run before attempting another BYOND run. The working prefix and Lutris runner remain unchanged.

## Cross-process GDI versus XRender result: 2026-08-14

The latest diagnostics identify where the pixels disappear.

The X11 stacking micro-test (`xrestack-order-test`) confirmed that the Wine-side sibling order is not simply reversed. The live DreamSeeker tree also showed the expected order: the 1258x696 `e33cfbfa.htm` document surface is above the full-size `a0284aed.htm` surface. Reversing the order again would therefore be the wrong fix.

The following runs were negative for the final nested composition:

```text
20260814-015119-game  ARGB child surfaces retained; form capture valid; game surface still map-only
20260814-020150-game  manual XComposite redirect; failed with BadAccess and was removed
20260814-020817-game  XRender named-pixmap copy; target readback remained map sand (0xc29b67)
20260814-021110-game  direct XRender drawable copy; same target readback (0xc29b67)
20260814-021910-game  normal Win32 probe; source GetDC saw the correct form pixels
```

The decisive Win32 probe output was:

```text
gdi_pixels corner=ffffff center=232c39 card=0e2743
```

That means Edge did render the form. The white page, dark card, and button are present when the foreign WebView HWND is read through its ordinary Windows DC. XRender/XComposite, however, does not provide the same usable image to the DreamSeeker presentation path. In beginner terms: the browser painted the picture, but Wine asked the Linux display server for the wrong copy of that picture.

An opt-in GDI copy path was added as a diagnostic (`WINE_FOREIGN_CHILD_SURFACES=gdi`). It proved that the copy call executes, but `NtUserGetDCEx` from the DreamSeeker presentation thread still read white for the foreign browser surface even though a separate normal `GetDC` probe read the form. This is consistent with a cross-process timing/ownership problem, not a missing HTML element. The GDI path is retained only as instrumentation and is not an upstream patch.

## Root-level ARGB overlay experiment: 2026-08-14

The strongest practical experiment was:

```bash
WINE_KEEP_CHILD_WINDOWS=1 \
WINE_WEBVIEW_ARGB=1 \
WINE_WEBVIEW_ROOT_SURFACE=1
```

This keeps the WebView's ARGB document surface as a root-level X11 overlay instead of asking the nested DreamSeeker child hierarchy to composite it. In run `test-results/20260814-024332-game`, the browser became viewable in all five post-advertisement captures, and the recursive X11 desktop capture contained the login form over the map. The captured form card is in the expected centered position once the normal game title bar is included.

The same mechanism also made the BYOND Hub WebView viewable in run `test-results/20260814-024541-hub`; the Hub and its `pagerhome.html` surface were nonblank. This is evidence for a Wine compositor/foreign-surface problem, not a BYOND server or HTML problem.

The root overlay is not yet safe to enable by default. It can lose normal child clipping and focus behavior, and the current source implementation still contains diagnostic code and several environment-controlled experiments. It is a valuable diagnostic workaround, not a finished upstream change.

## Current source direction

The next upstream-quality patch should generalize the root-level foreign-surface handoff without naming WebView2 or Chrome, preserve the Win32 child rectangle and clipping semantics, and include a small cross-process X11/Win32 regression test. The BYOND run is now the integration test:

```text
Hub nonblank:                         PASS with root overlay experiment
DreamSeeker map:                      PASS
WebView2 login DOM:                   YES
WebView2 controller failure:          NO
Nested final composite:               NOT FIXED
Root-level ARGB composite experiment: VISUALLY PROMISING
Upstream PR:                          NOT OPENED
```

The working prefix `/home/octavio/Games/byond32` and Lutris runner remain untouched. The custom source and runner under `/home/octavio/Downloads/linux-ports` are still a lab branch; no claim of a finished Wine fix should be made until the overlay behavior is tested for focus, resize, movement, and repeated login transitions.

## Root-overlay coordinate correction: 2026-08-14

WebView2 reconfigured the root-level X11 child back to `(0,0)` after Wine initially positioned it. A presentation-time alignment pass now moves root-level foreign surfaces to the actual game X11 origin. Run `20260814-025636-game` recorded the login document at `+0+28`, matching the 28-pixel game title-bar offset; the WebView was viewable in 5 of 6 post-advertisement captures and the login DOM remained `YES`.

A click at the username-field coordinates left `_NET_ACTIVE_WINDOW` reporting the DreamSeeker top-level. Because the WebView surface is an override-redirect child, that result does not prove that the click failed, but it also does not prove that focus works. Input routing, resizing, and moving the game remain mandatory follow-up tests before this can be called a user-ready workaround.

## Root-overlay input result: 2026-08-14 03:01

The interaction test used run `20260814-030157-game`:

1. Wait through the normal advertisement transition.
2. Click the aligned username field at X11 coordinates `(650,215)`.
3. Send the harmless text `test` through XTest.
4. Inspect the later embedded WebView capture.

The capture shows `test` in the Username field and the field's focus outline. Therefore the root-level ARGB overlay is both visually rendered and interactive. `_NET_ACTIVE_WINDOW` continued to name DreamSeeker because the WebView X11 surface is an override-redirect child; that property is not a valid success criterion for this path.

This makes the root-overlay mode a viable lab workaround while the nested-child implementation is being reduced into an upstream-quality patch. It does not yet prove resize/move behavior or that every BYOND WebView surface should be treated this way.

The final Hub regression with the corrected source was `test-results/20260814-030408-hub`:

```text
Hub frames:             4
Nonblank Hub frames:    4
WebView2 controller:    no failure
WebView viewable:       3 post-launch captures
```

## Full-page white overlay and map recovery: 2026-08-14 11:00–11:35

The next failure appeared after the HTML form became stable: a white 1258x696
WebView window covered most of the game, and one manual launch appeared to
crash. The white rectangle was not a second HTML page. It was the login
document's entire native X11 surface being displayed as an opaque root window;
the login card was only a small part of that surface.

The first attempted size filter ran too early. WebView2 creates the native
document as a tiny window and resizes it to 1258x696 later, so the filter could
not identify it in `SetParent`. The working sequence is now:

1. Preserve the native ARGB Chrome document as a root-level overlay.
2. During presentation, identify the final 1366x696 map document and
   reparent that full-size surface back under DreamSeeker.
3. Keep the smaller 1258x696 login document at the X11 root and apply an
   opt-in XShape bounding/input region around the 273x310 login panel.
4. Cache the shape and avoid repeated `XMoveWindow` calls when the geometry has
   not changed, so Wine does not continuously reconfigure the browser surface.

The lab flags for the Shinobi login layout are:

```bash
WINE_KEEP_CHILD_WINDOWS=1 \
WINE_WEBVIEW_ARGB=1 \
WINE_WEBVIEW_ROOT_SURFACE=1 \
WINE_WEBVIEW_ROOT_CLIP=login
```

The `login` clip is intentionally not enabled by default: its rectangle is
specific to the tested 1366x696 game layout and is not yet a generic Wine
compositor solution. The Hub must use the first three flags without
`WINE_WEBVIEW_ROOT_CLIP=login`.

Validation after the background reparent fix:

```text
20260814-110804-game  map frames 7/7 nonblank; WebView viewable 35/36; DOM YES; controller failure NO; app_exited=0
20260814-111624-game  full-size map document nested under DreamSeeker; login document root-level; app_exited=0
20260814-112542-hub   recursive desktop captures 5/5 nonblank; WebView viewable 5/5; app_exited=0
20260814-113251-game  map frames 8/8 nonblank; WebView viewable 34/35; DOM YES; controller failure NO; app_exited=0
```

The shaped-input test accepted the harmless text `test` and showed the field
focus outline. Some individual WebView captures during the click/repaint
transition were black before the form returned; this is still a browser
repaint/focus flicker to investigate, not a successful claim that every
interaction is perfect. The long runs did not show a DreamSeeker crash. The
remaining `BadWindow` messages came from a capture helper racing teardown,
not from an application exception.

The working prefix `/home/octavio/Games/byond32` and Lutris runner were not
modified. The custom runner and source remain under the lab workspace, and the
source is still WIP rather than a PR-ready generic Wine patch because the
current clip geometry is application-specific and resize/move/login-transition
coverage is incomplete.

## Stale advertisement cleanup and low-overhead stability validation: 2026-08-14 12:21–12:58

The next screenshot showed a new, more specific failure: the login card was
visible, but the BYOND advertisement remained as a large black/white window on
the left after the map had loaded. The cause was not another HTML failure. The
advertisement is a separate root-level X11 WebView window, outside the
DreamSeeker Win32 child-window tree that the first cleanup pass searched.

The first root-scan attempt was not sufficient: the map-only screenshots looked
correct, but the X11 inventory showed that the ad was still viewable. The
reason was a timing race. Wine could notice the map before the advertisement's
final native surface was mapped, then stop checking too early.

The current source detects the map WebView by its X11 class and final size,
then looks for a direct root child whose title contains `ad.html`. It keeps
checking at presentation time until that surface is found, caches the result,
and unmaps only the identified advertisement surface. It leaves the associated
HWND alive so BYOND can finish its ordinary cleanup; this is a presentation
fix, not forced process termination. The cached lookup avoids scanning all root
children on every frame after the ad has been handled.

The test harness also gained two diagnostic modes:

```text
BYOND_CAPTURE_IMAGES=1       save visual captures
BYOND_CAPTURE_INVENTORY=1    query each WebView's geometry/map state
BYOND_CAPTURE_IMAGES=0       skip ImageMagick captures
BYOND_CAPTURE_INVENTORY=0    skip per-WebView xwininfo/xprop queries
```

The last two settings are important for a delayed-crash test. Inspecting ten
browser windows every second can itself consume enough CPU to make a fragile
game look worse. The low-overhead run still records the X11 tree, all Wine
processes, CPU/RSS, uptime, and WebView2 log output.

### Evidence

| Run | Test | Result |
| --- | --- | --- |
| `20260814-122126-game` | First root-scan attempt | Negative control: the ad remained viewable in the later X11 inventory. The map-only capture was therefore misleading. |
| `20260814-122852-game` | First visual regression | Map captures were nonblank, but the ad also remained viewable. This exposed why capturing the map child alone is not enough. |
| `20260814-124739-game` | 75-second lifecycle/visual run after the late-check fix | 10/10 nonblank map captures, login DOM detected, no controller failure; later inventory samples reported the ad as unmapped. |
| `20260814-125101-game` | 75-second lifecycle run after caching the ad lookup | `ad_viewable_samples=2`, `ad_unmapped_samples=17`, map detected in 17 samples; DreamSeeker alive in all 23 samples; login DOM detected. |
| `20260814-125235-game` | 180-second low-overhead stability run on the final source | `app_exited=0`; DreamSeeker alive in all 156 process samples; map detected in 111 samples; login DOM detected; controller failure absent; maximum WebView process count 10. |
| `20260814-125654-hub` | 45-second Hub visual regression on the final source | 13/13 nonblank Hub captures; WebView visible; no controller failure; `app_exited=0`. |

The final 180-second run produced three existing
`wined3d_swapchain_resize_buffers Something's still holding back buffer 0`
messages. It produced no segmentation fault, unhandled exception, fatal Wine
error, or WebView2 controller-disconnect error. Those resize messages remain a
real warning to investigate; the test shows that they did not crash this run.
After startup settled, DreamSeeker stayed around 334 MB RSS and the main
WebView process around 284 MB RSS; the process count did not grow beyond ten.
DreamSeeker was using roughly one CPU core late in the run, so this test rules
out a simple runaway process count or monotonic memory leak, but it does not
prove that rendering is perfectly smooth under all game activity.

The visual evidence is stored in the test-results directory. In particular,
`20260814-124739-game/screens/game-0017.png` shows the recovered map and the
corresponding `embedded-0017.png` shows the login card. The small 273x310 white rectangle
around the card is the HTML document's own page background, also present in the
reference screenshot supplied for this game. It is not the old 1258x696 opaque
WebView that covered most of the map. Removing that small rectangle would be a
separate transparency/HTML-background problem and should not be mixed into the
surface-order fix.

### Reproduce the lab test

These commands use only the isolated prefix and custom Wine install; the
working Lutris prefix and runner remain untouched:

```bash
cd /home/octavio/Downloads/linux-ports
env DISPLAY="${DISPLAY:-:0}" \
  BYOND_PREFIX=/home/octavio/Downloads/linux-ports/byond32-upstream-baseline \
  BYOND_RUNNER=/home/octavio/Downloads/wine-build-upstream32/install \
  BYOND_CAPTURE_IMAGES=1 BYOND_CAPTURE_INVENTORY=1 \
  WINE_KEEP_CHILD_WINDOWS=1 WINE_WEBVIEW_ARGB=1 \
  WINE_WEBVIEW_ROOT_SURFACE=1 WINE_WEBVIEW_ROOT_CLIP=login \
  ./tools/byond-test-runner.sh --game byond://198.166.109.89:3336 \
  --duration 180 --interval 1
```

For a clean delayed-crash measurement, add
`BYOND_CAPTURE_IMAGES=0 BYOND_CAPTURE_INVENTORY=0`. For the Hub, use
`--hub` and omit `WINE_WEBVIEW_ROOT_CLIP=login`; that clip is specific to the
tested Shinobi layout and is not a generic Wine setting.

This is a strong lab result, not yet an upstream-ready patch. The remaining
work is to replace the BYOND-specific title/geometry detection with generic
foreign-surface handoff semantics and add resize, move, focus, and repeated
WebView transition tests before proposing a Wine pull request.

## Card-only clipping, WebView process pressure, and five-minute validation: 2026-08-14 14:00–15:00

The next iteration addressed two different symptoms separately:

1. The small white rectangle around the login card was the page background of
   the 1258-pixel-wide login WebView, not part of the HTML card. The new
   `WINE_WEBVIEW_ROOT_CLIP=login-card` mode applies an XShape bounding and input
   region around the tested 220x258 card. The page background is no longer
   visible in the embedded capture, while both password-recovery links remain
   visible.
2. The delayed lag/crash behavior correlated with WebView2 creating ten
   browser processes and using roughly 2.3 GB of resident memory. Adding the
   Chromium/WebView2 argument `--renderer-process-limit=1` reduced the tested
   workload to seven WebView-related processes and roughly 1.55–1.6 GB of
   resident memory. It is a memory-pressure mitigation, not a Wine compositor
   fix.

The second argument tested during the first experiment was
`--disable-site-isolation-trials`. It was not necessary: the game passed
without it, so it is deliberately excluded from the recommended configuration.
That avoids an unnecessary browser security tradeoff.

### Validated DreamSeeker settings

Keep the existing base browser arguments and append only the renderer limit:

```text
WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--disable-gpu --disable-gpu-sandbox --disable-gpu-compositing --disable-d3d11 --disable-dwm-composition --use-angle=d3d9 --disable-background-networking --disable-metrics --disable-metrics-repo --disable-crash-reporter --disable-component-update --disable-features=msEdgeMetrics,msWebOOUI --renderer-process-limit=1
WINE_KEEP_CHILD_WINDOWS=1
WINE_WEBVIEW_ARGB=1
WINE_WEBVIEW_ROOT_SURFACE=1
WINE_WEBVIEW_ROOT_CLIP=login-card
```

`login-card` is intentionally a DreamSeeker/Shinobi layout workaround. It is
not generic enough to enable for every BYOND HTML page. For the Hub, use the
same settings but omit `WINE_WEBVIEW_ROOT_CLIP` (or set it to `0`). If Hub and
DreamSeeker are separate Lutris entries, put the card clip only on the
DreamSeeker entry. The isolated test prefix and custom Wine install used here
were:

```text
BYOND_PREFIX=/home/octavio/Downloads/linux-ports/byond32-upstream-baseline
BYOND_RUNNER=/home/octavio/Downloads/wine-build-upstream32/install
```

The working prefix `/home/octavio/Games/byond32` and Lutris-managed runner were
not changed.

### Evidence

| Run | Test | Result |
| --- | --- | --- |
| `20260814-143020-game` | 75-second visual run with card clip and renderer limit | 14/14 nonblank map captures; card-only form has no page rectangle; DOM YES; controller failure NO. |
| `20260814-143218-game` | Five-minute low-overhead DreamSeeker run | `app_exited=0`; 256/256 process samples with DreamSeeker alive; DOM YES; controller failure NO; max seven WebView-related processes. |
| `20260814-144023-hub` | 45-second recursive Hub run with renderer limit | 13/13 nonblank recursive Hub captures; WebView visible; `app_exited=0`; controller failure NO. |
| `20260814-144547-game` | Click/type test after the advertisement | Username field accepted `test` and retained its focus outline; 16/16 nonblank map captures; DOM YES; controller failure NO. |
| `20260814-144857-game` | Two-minute root-overlay-off control with renderer limit | DreamSeeker remained alive; CPU was about 75% at two minutes versus about 78% with the overlay enabled, showing that the overlay is not the main source of the remaining CPU cost. |

The five-minute run ended with DreamSeeker around 337 MB RSS. The listed
WebView browser, utility, GPU, and renderer processes were approximately
1.55–1.6 GB combined and did not show monotonic growth. The older five-minute
configuration reached ten WebView-related processes and approximately 2.3 GB
combined WebView RSS, leaving substantially less available memory. This is
consistent with memory pressure contributing to the delayed hangs/crashes.

The run still prints three existing
`wined3d_swapchain_resize_buffers Something's still holding back buffer 0`
errors. They did not cause an exit in the five-minute test, but they remain a
separate WineD3D issue. DreamSeeker also remains CPU-heavy because its map is
rendered through the 32-bit wined3d path; the WebView/compositor work is only a
small part of that CPU total. Further performance work should profile the map
renderer rather than weakening the WebView isolation settings.

The recursive Hub capture is the reliable visual check for this setup. The
direct active-window helper can capture a white helper surface even in a
known-good run; the analyzer now recognizes the Hub's 920x480 WebView surface
and avoids reporting that capture artifact as a browser failure.

The source changes are still WIP. The card geometry and title-based ad cleanup
must be replaced with generic foreign-surface semantics, and resize, move,
focus, and repeated login-transition tests are still required before opening a
Wine pull request.

## Best candidate solution found, then discarded: 2026-08-15

The best direction was a generic X11 child-window handoff in Wine, not a BYOND
configuration trick and not a root-window overlay. The important observation
was that WebView2 changes a browser-owned HWND from a top-level window into a
child of a DreamSeeker HWND owned by another process. Wine normally destroys
the top-level X11 window during that transition. That leaves the browser DOM
alive but separates its native presentation surface from the game window.

The candidate implementation did the following:

1. On a cross-process `SetParent`, preserved the existing X11 whole window and
   reparented it under the host application's X11 window.
2. Tracked that state as a foreign child, so only these explicitly preserved
   windows used child-relative geometry synchronization. Ordinary Wine child
   and XEMBED windows kept the normal path.
3. Reparented the surface back to the X11 root when the Win32 window became a
   top-level window again, including visibility and host-parent cleanup.
4. Resolved the actual foreign drawable for DC/client-surface operations and
   matched an existing ARGB X11 visual when the browser surface required it.

The final candidate deliberately removed the earlier lab-only behavior:

- no root-window WebView overlay;
- no `XRaiseWindow`, global topmost behavior, or root clipping;
- no BYOND window titles, `ad.html` checks, login-card coordinates, or
  application-specific geometry;
- no `WINE_KEEP_CHILD_WINDOWS`, `WINE_WEBVIEW_ROOT_SURFACE`,
  `WINE_WEBVIEW_ROOT_CLIP`, or similar runtime switches in the Wine code;
- no GDI/XRender copying of a foreign browser window.

The candidate's net functional files were:

```text
dlls/winex11.drv/window.c   cross-process reparenting and child geometry
dlls/winex11.drv/x11drv.h  foreign-child state bit
dlls/winex11.drv/init.c    client-surface visual matching
dlls/winex11.drv/bitblt.c  alpha handling for the selected surface
dlls/win32u/opengl.c      DC/pixel-format fallback for foreign windows
```

The changed objects compiled and linked. A 45-second Hub run stayed alive and
showed `pagerhome.html` below the BYOND top-level X11 window. A 180-second
DreamSeeker run stayed alive and showed the map and embedded WebView windows
below the DreamSeeker top-level window. Those runs established correct X11
parentage and removed the global-overlay behavior, but they did not prove
production-quality pixel presentation or long-term smoothness. The next
visual run stopped at BYOND's network-error splash because Linux could not
reach `byond.com`; that result is invalid for judging the compositor.

The test helper was corrected after that run: X11 root-tree lines contain
coordinates relative to the parent and absolute coordinates, and the helper
had been using the former. It now queries the ad window's absolute position
with `xwininfo` and clicks only while the actual `ad.html` document exists.
This is test-harness correctness, not a Wine workaround.

Therefore this candidate is not being presented as a finished Wine fix. It is
the most useful upstream investigation result: it identifies the generic
cross-process child-surface lifetime/parenting area to discuss with Wine
developers, while leaving the Wine checkout clean and upstream-compatible.

## Restoration state

The only Wine repository is:

```text
/home/octavio/Downloads/wine-upstream
```

Its `master` and `origin/master` point to the upstream commit recorded in the
source-inspection section. The `winex11-cross-process-child` branch was
reset to that same commit, so it contains zero experiment commits. Files under
`test-results/` are evidence from the discarded candidate and are ignored; they
are not Wine source changes.

The Wine repository and generated build were subsequently moved out of this
project directory so that `linux-ports` contains only the BYOND investigation:

```text
/home/octavio/Downloads/wine-upstream/
/home/octavio/Downloads/wine-build-upstream32/
/home/octavio/Downloads/linux-ports/
```

## Archived standalone-reproducer workflow

The following workflow documents the discarded experiment for historical
context only. The harness and its generated results have been removed because
they never reproduced the real DreamSeeker hover-flicker symptom. Do not use
this section as the current test procedure.

The next phase has six deliberately small steps. The purpose is to make each
result comparable and to avoid changing Wine and the test harness at the same
time.

1. **Keep any future diagnostic program outside the Wine source tree.** The
   removed harness used one local
   HTML page, so it does not depend on the BYOND server, advertisements, or
   the game's JavaScript. It is a diagnostic program, not a Wine patch.
2. **Run Wine's existing tests unchanged first.** We will use a separate
   test-enabled build of the clean upstream source and record the relevant
   `user32`/`winex11` results. Existing Wine tests must remain unchanged; the
   reproducer must not be inserted into them just to make the experiment pass.
3. **Make one narrowly scoped Wine change.** Only after the clean baseline is
   recorded will we modify the one subsystem implicated by the reproducer:
   X11 handling of a cross-process Win32 child window and its graphics
   surface. No BYOND-specific titles, coordinates, overlay windows, or runtime
   switches belong in Wine.
4. **Run the same existing tests again.** The post-change test run must use the
   same command and test-enabled build configuration as the baseline, followed
   by the same instant reproducer. This separates a general Wine regression
   from a change that only affects the WebView2 scenario.
5. **Require every pre-existing test to keep passing.** A visual improvement
   is not enough if the change breaks ordinary `SetParent`, child-window,
   repaint, or X11 tests. Any failure means we stop and understand that failure
   before continuing; we do not hide it with a skip or modify the old test.
6. **Add a Wine regression test only when the failure is deterministic and
   portable.** A real WebView2 runtime is normally unsuitable for Wine's test
   suite because it is an external proprietary runtime and may not exist in
   Wine CI. If the reproducer proves a smaller Win32/X11 behavior that can be
   tested on Windows and in Wine without WebView2, we add that minimal test to
   the appropriate existing suite. Otherwise the WebView2 program remains an
   external reproducer attached to the eventual Wine report/MR.

Every run should preserve four artifacts: the exact Wine revision and build
configuration, the reproducer's stdout/stderr, an X11 window-tree snapshot,
and a screenshot or other presentation check. This gives us a before/after
record that can be reviewed without guessing which experiment produced it.

## Archived standalone-reproducer results (removed)

The removed external harness used the 32-bit WebView2 loader already present
in the isolated BYOND baseline prefix and an official Microsoft WebView2
header extracted outside this repository. No WebView2 SDK files or runtime
files were copied into Wine's source tree.

On 2026-08-15, both stages below were run with the clean Wine 11.15 source
revision `e99fc2f7587db7bc186293e9949fe0d3695cd430`, the external 32-bit build
at `/home/octavio/Downloads/wine-build-upstream32/`, and the isolated prefix
`byond32-upstream-baseline/`.

### Stage A: GDI host

The host first used ordinary GDI painting to remove the game's map renderer
from the equation. WebView2 loaded the local page, then the child process moved
the controller to the host HWND, resized it, restored it, and repeated the
handoff. Every recorded WebView2 HRESULT was `S_OK`; the login card appeared
inside the map window.

This stage passed. It proves that the loader, local HTML, controller creation,
and basic cross-process parent handoff can work in this prefix.

### Stage B: D3D9 host

The same harness was then changed only at the host renderer: the fake map was
drawn by a real Direct3D 9 device and presented every 16 ms. The WebView2
source, HTML, handoff order, bounds, prefix, Wine build, and browser flags were
unchanged.

The longer run held the final WebView2-on-host state for 20 seconds. Its
generated result directory was removed with the harness after review:

- `D3D9 CreateDevice` returned `S_OK`.
- Environment creation, controller creation, navigation, both parent
  handoffs, resizes, and `NotifyParentWindowPositionChanged` all returned
  `S_OK`.
- The HTML card remained visible over the animated D3D9 map; the page's own
  animation advanced during the hold.
- Wine exited with status 0. The log line saying the child was still active is
  the host's deliberate cleanup after the fixed sequence, not a crash.
- The only unrelated errors were the expected no-Vulkan notice and the
  missing `StorageApplicationPermissions` activation-class notice; neither
  prevented the test from rendering or completing.

This stage also passed. It is a negative control, not proof that the BYOND bug
does not exist: the real DreamSeeker may use a different HWND hierarchy,
surface type, timing sequence, or WebView2 controller setup. It does mean that
the current evidence is not sufficient to justify a Wine patch.

One important observation from the X11 trees is that Edge creates several
helper windows in the `msedgewebview2.exe` process. Those helper windows can
appear as root-level X11 entries even while the visible controller content is
correctly presented inside the host. We must therefore correlate the tree with
the screenshot and the controller HWND; treating every Edge helper window as
the visible HTML surface would give a false diagnosis.

No further standalone-harness revision is planned. The next investigation step
is to capture and analyze the real DreamSeeker/WebView2 presentation path. No
Wine source change is warranted until that failing transition is identified.

## DirectComposition z-order experiment rejected: 2026-09-11

### Expected behavior

The embedded HTML surface should remain visible above the DreamSeeker map. A
repaint should update pixels without repeatedly changing the surface's child
window stacking order.

### Experiment

The experimental bridge in `dlls/dcomp/target.c` previously called
`SetWindowPos(surface, HWND_TOP, ...)` after every pixel copy. The experiment
stored the last position, size, and visibility state. It raised the surface on
its first publication, used `SWP_NOZORDER` for later geometry changes, and
skipped `SetWindowPos` when nothing changed.

This changed only the experimental DComp bridge. It did not change WebView2
arguments, the renderer, shared-resource code, or the X11 driver.

### Observed result

The owner reported that the subsequent recording showed only the map and no
HTML panel. The recording was
`/home/octavio/Videos/Screencasts/Screencast_20260911_140814.mp4`.

The trace run `20260911-141059-game` showed that the HTML presentation path was
active despite the missing pixels:

- DirectComposition created and repeatedly published a `1258x740` surface,
  which matches the main WebView surface.
- Three additional surfaces of `700x550`, `17x17`, and `1x1` were also
  published.
- The destination parent was reported as visible.
- There were no failed pixel copies, failed `UpdateLayeredWindow` calls, or
  failed surface-position calls.
- The live X11 tree contained viewable WebView2 child windows, but the final
  screen still showed only the map.

### Conclusion

The experiment is rejected. It did not prove that repeated `HWND_TOP` calls
were the root cause. In the current synthetic-surface design, repeated raising
may be compensating for a deeper X11 parent or stacking problem. Suppressing
it can leave the published surface present but not visible in the final image.

The smallest current causal model is therefore:

1. WebView2 creates real browser content and asks DirectComposition to present
   it.
2. Wine copies that content into a synthetic layered child window.
3. Wine reports successful publication, but the X11 result can still show only
   the D3D9 map.
4. The missing behavior is in the relationship between the synthetic surface,
   its foreign parent, and the map's X11 child order. It is not established
   that `SetWindowPos` frequency alone causes the failure.

The source was restored to the pre-experiment behavior and the restored DComp
DLL was rebuilt. The installed custom runner still needs to be replaced with
that rebuilt DLL before it is called a restored baseline. Until then, a test
must explicitly load the rebuilt DLL from `/tmp` with `WINEDLLPATH=/tmp`.

No new z-order patch should be attempted until the next trace records the X11
window identifier corresponding to the synthetic surface and its actual parent
and sibling order.

### Follow-up recording and stronger D3D11 clue

The recording
`/home/octavio/Videos/Screencasts/Screencast_20260911_141257.mp4` is 4.70
seconds long at 60 frames per second. It alternates between the map alone and a
centered dark login panel. The complete panel appears or disappears as one
unit, rather than individual controls being repainted independently.

The visible panel has `SIGN IN`, `CREATE ACCOUNT`, `CREATE NEW ACCOUNT`, and
green controls. That appearance matches the separate backup login window that
the owner identified earlier. The embedded form has a different appearance,
including a white outer surface and `SIGN IN` and `REGISTER` controls. The
recording therefore proves intermittent window visibility, but the picture
alone cannot identify which of the two login windows is visible. The owner's
manual observation remains the authority for the embedded form's symptom.

The trace from the same run provides a stronger implementation clue:

```
warn:dxgi:dxgi_resource_inner_QueryInterface {9d8e1289-d7b3-465f-8126-250e349af85d} not implemented, returning E_NOINTERFACE
warn:d3d11:d3d11_texture2d_QueryInterface {9d8e1289-d7b3-465f-8126-250e349af85d} not implemented, returning 0x80004002
```

The GUID is `IDXGIKeyedMutex`. This is a DirectX synchronization interface
used when two processes share a GPU texture. One process acquires a key before
reading or writing the texture, then releases a different key so the other
process can take its turn. WebView2 can depend on this contract when it shares
its D3D11 presentation surface with the host.

In this run, the DComp bridge published the main `1258x740` surface 125 times
and the `700x550` surface 127 times. That proves that the copy and publication
workaround was active. It does not prove that the original WebView2 shared
surface contract was working. The repeated `IDXGIKeyedMutex` failures show that
the D3D11 texture interface expected by the browser is still missing. The
`GetDeviceRemovedReason` stub warnings are also present, but they do not by
themselves establish that device removal caused the visual failure.

This recording predates the source-tree restoration described above. It used
the installed custom runner while the rejected z-order experiment was still
installed. The source tree is restored, but the installed runner is not yet a
baseline. The next single discriminating test is to run the same game while
explicitly loading the rebuilt baseline `dcomp.dll` from `/tmp`, then compare
the owner's manual result and the keyed-mutex warnings. No further source
change should be made until that comparison is complete.

## Restored baseline confirmation: 2026-09-11

### Expected behavior

The embedded HTML panel should remain continuously visible above the map while
the game is running.

### Test state

Run `20260911-145526-game` used the isolated baseline prefix and the custom
Wine 11.15 runner. The rebuilt baseline `dcomp.dll` was installed in the
runner before launch. Its SHA-256 hash matched `/tmp/dcomp.dll` exactly. The
previous experimental runner DLL was preserved as
`/tmp/dcomp-install-before-baseline-20260911.dll`.

No Wine source change was made for this run. The test changed only which
already-built DComp DLL the runner loaded.

### Observed result

The owner manually confirmed that the embedded HTML panel still flickers in
the same way. The problem therefore survives restoration of the original
per-frame z-order behavior.

The baseline run logged 76 lines containing the `IDXGIKeyedMutex` GUID and 175
`GetDeviceRemovedReason` stub warnings. The keyed-mutex warnings are still
coming from the D3D11 and DXGI layers after the z-order experiment was removed.

### Conclusion

The z-order suppression experiment is definitively rejected as a fix. It did
not cause the persistent flicker, and restoring the old behavior did not
remove it. The next source investigation should focus on the missing D3D11 and
DXGI shared-texture synchronization contract, especially `IDXGIKeyedMutex`,
rather than on more `SetWindowPos` or `WINE_D3D_CONFIG` variations.

## Keyed mutex source experiment: 2026-09-11

### Expected behavior

A D3D11 texture created for keyed cross-process sharing should expose
`IDXGIKeyedMutex`. The producer and consumer use that interface to take turns
using the shared texture. An ordinary shared texture should not expose that
interface.

This distinction is part of the D3D11 contract. Microsoft documents that
`D3D11_RESOURCE_MISC_SHARED` and
`D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX` are separate resource modes, and the
keyed mutex is associated with the latter mode:

- [D3D11_RESOURCE_MISC_FLAG](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_resource_misc_flag)
- [IDXGIKeyedMutex](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nn-dxgi-idxgikeyedmutex)

### Source change

The Wine source already had low-level keyed mutex creation, opening, acquire,
and release operations. The D3D11 texture object did not expose those
operations through `IDXGIKeyedMutex`, and its shared-resource descriptor did
not carry the mutex handle to another process.

The experiment added the missing D3D11 interface wrapper, created a keyed mutex
for textures explicitly marked with
`D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX`, reopened the handle for imported
textures, and carried the handle through the existing Wine shared-resource
descriptor. It did not change window ordering, DirectComposition, WebView2
arguments, or the X11 driver.

### Focused test result

The rebuilt D3D11 and WineD3D libraries were installed together after an
initial mismatch was found between their shared descriptor layouts. The
focused D3D11 test then passed the relevant checks:

1. A keyed shared texture answered the `IDXGIKeyedMutex` query.
2. Local acquire and release completed.
3. A child process imported the shared texture and received the mutex handle.
4. The child process acquired and released the imported mutex.

The complete D3D11 suite still has unrelated existing failures. Those failures
were not used as evidence for or against this experiment.

### Real SOA result

The native WebView2 run used the isolated Windows 10 prefix
`20260911-182841-game`. It loaded the rebuilt D3D11, DXGI, DirectComposition,
and WineD3D libraries. SOA still queried `IDXGIKeyedMutex` on several texture
objects and still received `E_NOINTERFACE`.

The source trace showed that those objects were ordinary shared textures, not
textures created with `D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX`. Returning no
keyed mutex for those objects is therefore consistent with the documented API
contract. Making every shared texture expose a mutex would be an incorrect
change, not a fix.

The same run reached `CreateSwapChainForComposition` successfully. No
`RPC_E_DISCONNECTED` error was recorded. This does not prove that the HTML
surface was presented correctly, because the current DirectComposition bridge
still copies pixels into a synthetic layered child window.

### Decision

This experiment is a valid partial Wine API implementation and its focused
test is useful regression coverage. It is not the SOA fix. The SOA keyed mutex
warnings are now treated as optional probes on ordinary shared textures, not
as proof that SOA needs every texture to expose `IDXGIKeyedMutex`.

No further keyed mutex change should be made for ordinary shared textures. The
next single investigation is an instrumented SOA run with the `dcomp` trace
channel enabled. That run must identify the DirectComposition calls, target
window, surface publication, and commit timing before another source change is
chosen.

## DirectComposition publication and parent lifetime trace: 2026-09-11

### Expected behavior

The embedded browser surface should remain visible above the D3D9 map while
the browser changes pages, hides old targets, or reparents its child windows.
Publishing a new frame should not destroy the destination relationship or leave
the surface underneath the map.

`UpdateLayeredWindow` is the Win32 operation used by the current Wine bridge to
copy the composed pixels into its temporary layered child window. A successful
return means that Wine accepted the pixel copy. It does not, by itself, prove
that the X11 window manager placed the result at the correct visual layer.

### Test state

Two diagnostic runs used the isolated Windows 10 prefix, the custom Wine
runner, and the current source build. They did not modify the everyday BYOND
prefix or the game. The first run enabled DirectComposition, D3D, DXGI, and
X11 warnings. The second run added the `win` trace channel, which is the
channel used by Wine's window implementation. `win32u` was not the correct
channel name for this source tree.

The runs were:

```
test-results/20260911-190024-game
test-results/20260911-190659-game
```

### Observed behavior

The first run recorded two failed publications while the main target changed
from `1258x740` to `1358x840`:

```
target_update_surface Failed to publish composition surface, error 87.
do_composite composite target 00010682 destination 00010640 visible 1 offset 0,0 size 1358x840 copied 0.
```

The second run did not reproduce those two publication failures. That makes
the resize failure timing dependent rather than a deterministic rejection of
the pixel copy.

The second run did expose a different and more specific failure. Target
`00010604` first used parent `000105d2`, then was assigned destination
`000205a2`. The destination itself was later reparented to `00020074`. Near
the end of the run, the DComp bridge tried to create a new temporary layered
child under `000205a2`:

```
WIN_CreateWindowEx ... L"static" ... parent=000205A2
NtUserCreateWindowEx ... parent 0x205a2 ...
warn:win:create_window_handle error 87 creating window
warn:dcomp:target_create_surface Failed to create composition surface, error 87.
```

The same log still showed `GetWindowRect` succeeding for `000205A2`. This is
important. A client-side `IsWindow` or `GetWindowRect` check can say that a
handle still exists while the Wine server refuses a new child because the
parent or one of its ancestors is in teardown state.

The server's `create_window` handler returns error 87 when the requested parent
or an ancestor is marked orphaned. An orphan is a Wine server window whose
parent relationship has been detached during destruction. The trace cannot
read that private server flag, so this is strong source-level evidence, not a
direct observation of the flag.

For the surfaces that already existed, the `win` trace showed successful
`UpdateLayeredWindow` operations. They returned `1`. The accompanying value
`error 1400` was a stale last-error value, not a failed call, because Win32
does not require a successful function to clear the previous error.

The run also reported X11 `BadWindow` errors for resources such as
`0x3e00005` soon after the failed child creation. This is consistent with a
window resource being destroyed while another operation still refers to it,
but the trace does not prove which operation caused the X11 error.

### Current model

The current DComp implementation is a compatibility bridge. It does not hand
the browser's native DComp visual tree directly to X11. It copies the
composed image into a temporary layered child window and places that window
under the browser destination.

The smallest causal story that explains the evidence is:

1. WebView2 creates browser windows and changes their parent and visibility as
   pages and controls change.
2. Wine's DComp thread observes one of those destinations and tries to create
   or update a temporary layered child.
3. The destination can be reparented or hidden between the visibility check
   and the server operation that creates or updates the child.
4. The server rejects a new child whose parent is already considered orphaned,
   even though a client query can still see the old handle.
5. Existing surfaces can continue to publish successfully while the affected
   surface disappears. This matches an HTML panel that appears intermittently
   or remains below the map.

This model is more specific than the earlier theory that repeated
`SetWindowPos` alone caused the flicker. The earlier z-order experiment was
restored and the owner still observed the same SOA flicker, so that theory is
rejected as a complete fix.

### What this proves and what it does not prove

The trace proves that the current bridge can copy pixels successfully for
existing layered surfaces. It proves that at least one helper-window creation
can fail with error 87 during a browser parent transition. It also proves that
the first resize publication failure is not reliable enough to use as the
only reproduction condition.

It does not prove that every SOA flicker is caused by the failed creation of
`00010604`. The user-visible SOA panel may use another target, and the
synthetic bridge can also lose stacking or synchronization after a successful
publication.

### Next single source investigation

Do not change the Wine server's orphan check. That check prevents new windows
from being attached to a window that is being destroyed.

The next source experiment should be limited to the DComp bridge in
`dlls/dcomp/target.c`. It should make destination transitions explicit and
safe: resolve the current destination, stop using the old temporary surface
when the destination changes, and defer creation until the destination is
still valid and visible at the point of creation. The experiment must add
traces for the destination, its root, visibility, current temporary surface,
and the result of each create or destroy operation.

The result will distinguish two cases. If the helper creation failure and the
SOA flicker disappear, the bridge was racing the browser's window lifetime. If
the helper remains stable but SOA still flickers, the next investigation must
move to X11 stacking or frame synchronization. No configuration matrix or
second source change should be mixed into that test.

## Parent reuse source experiment: 2026-09-11

### Expected behavior

When a browser destination changes parent, the bridge should preserve its
temporary surface when possible. A short transition should not require a new
child window, because creating that child can overlap with destruction of the
old browser parent.

### Source change

The experiment changed only `dlls/dcomp/target.c`. On a destination change it
kept the existing layered surface and called `SetParent` instead of destroying
the surface and creating another one. It also discarded a surface handle if
the parent had already destroyed it. The Wine server's orphan check was left
unchanged.

The previous installed DComp DLL was saved as
`/tmp/dcomp-install-before-parent-lifetime-20260911.dll` before the build.

### Test result

Run `20260911-192502-game` completed with the application still running at the
end of the test. The change did not remove the failure:

```
destroy_window 0x20072
WIN_CreateWindowEx ... L"static" ... parent=00020072
warn:win:create_window_handle error 87 creating window
warn:dcomp:target_create_surface Failed to create composition surface, error 87.
```

There were no successful reparent traces and no failed `SetParent` traces.
The new run therefore reached a create path where the old temporary surface
was already absent, probably because its former parent had removed it. The
reuse logic could not help in that state.

The run produced one composition-surface creation failure, no publication
failure, and two X11 `BadWindow` reports. Existing surfaces continued to
publish. This is not a Gate C success and the owner must not treat it as a
visual fix.

### Decision

This experiment is rejected as insufficient. Preserving an existing helper
surface is useful defensive behavior, but it cannot solve the case where the
browser destroys that helper along with its old parent before the compositor
thread observes the transition.

The experiment must be removed before the next source test so that two source
changes are not active at the same time. The next investigation should explain
why the bridge selects a parent that is being destroyed and whether it can
compose into a stable host window without creating a child under that
transient browser window.

## Target identity logging run: 2026-09-11

### Purpose

The previous warning named the rejected parent but not the DirectComposition
target that requested it. A logging-only build added the target handle, parent
handle, root handle, temporary surface, and size to the DComp state trace. It
did not change window creation, publication, or stacking behavior.

### Result

Run `20260911-193128-game` completed with the application still running. This
particular run had no composition-surface creation failure, no publication
failure, no error 87 from window creation, and no X11 `BadWindow` report.
That negative result confirms that the failure is timing dependent.

The trace repeatedly showed browser targets with root handle `0001F011E`.
The same run's window tree showed the WebView windows inside the SOA
DreamSeeker window. The root handle is therefore a strong candidate for the
stable host window, while the immediate parent handles such as `0001059E` and
`0001062C` are hidden browser intermediates. This conclusion is based on the
matching window hierarchy and handle traces. It is not a new claim about how
WebView2 is implemented on Windows.

### Next discriminating test

The next single source experiment will make the DComp bridge publish its
temporary layered child under the target's stable root window rather than the
transient immediate parent. The position calculation will be changed at the
same time so the surface keeps its current screen location. The test will
check whether the helper survives browser child replacement and whether its
X11 child remains inside the SOA window instead of becoming a separate window.

The logging-only change has been removed, and the installed runner has been
restored to the baseline DComp DLL before that test.

## Stable-root destination experiment: 2026-09-11

### Hypothesis

The immediate browser parent can be replaced during navigation. The stable
top-level SOA window should be a safer parent for the temporary composition
surface.

### Source change

The DComp bridge in `dlls/dcomp/device.c` was changed to select
`GetAncestor(parent, GA_ROOT)` for child targets. The position was converted
from screen coordinates to the root window's client coordinates. A coordinate
initialization error was found during the first attempt and corrected before
the valid run.

### Valid test result

Run `20260911-194158-game` completed with the application still running. The
main browser surface was published under root `0037011E` at ordinary positions,
including `54,0` for the `1258x740` content. The change therefore selected a
stable host for at least the visible browser target.

It did not fix the bridge. The run still recorded one child creation failure,
two publication failures, and two X11 `BadWindow` reports. The publication
failures occurred when the surface changed to `1358x840`:

```
NtUserUpdateLayeredWindow ... size 1358x840 ... ret 0 error 87.
target_update_surface Failed to publish composition surface, error 87.
```

Another target still attempted creation under immediate parent `0001059E`,
which had no usable stable root at that moment:

```
WIN_CreateWindowEx ... size 1366x740 parent=0001059E
warn:win:create_window_handle error 87 creating window
```

The stable-root choice therefore avoids the transient parent only for targets
whose ancestor chain reaches the game root. It does not solve every target,
and it does not solve the resize publication failure. The source change and
its installed DLL are rejected as a complete fix.

### Next source investigation

The DComp bridge creates its GDI bitmap only when the temporary surface is
first created. It never recreates that bitmap when the incoming swapchain
size changes. The next single source change will keep the temporary window
and recreate only its backing bitmap and device context when the size changes.
This should remove the known `1358x840` publication error without changing
parent selection or stacking behavior.

## Bitmap size source experiment: 2026-09-11

### Expected behavior

The temporary layered window needs a pixel buffer with the same width and
height as the incoming WebView frame. A GDI bitmap is that pixel buffer in the
current bridge. If the browser changes its frame size, the bridge must replace
the bitmap before copying pixels and calling `UpdateLayeredWindow`.

The child window, its parent, and its stacking relationship should remain
unchanged during this operation. Only the backing bitmap and its device
context should be recreated.

### Source change

The change was limited to `dlls/dcomp/dcomp_private.h` and
`dlls/dcomp/target.c`. Each composition target now records the bitmap width
and height. When the incoming frame dimensions differ, the bridge releases
the old device context and bitmap, creates a matching top-down 32-bit bitmap,
and keeps the existing layered window. The build completed and the resulting
`dcomp.dll` was installed in the isolated custom runner.

This experiment did not change the parent selection, z-order calls, WebView2
arguments, Wine prefix, or X11 driver.

### Observed result

Run `20260911-194615-game` completed with the application still running. It
recorded 762 successful `UpdateLayeredWindow` calls and no failed publication,
no error 87, and no failed composition-surface creation.

However, this run did not send a `1358x840` frame through the DComp bridge:

```text
DComp publications at 1358x840: 0
UpdateLayeredWindow return 0: 0
UpdateLayeredWindow return 1: 762
bitmap resize traces: 0
```

The ten `1358x840` lines in the same log belong to ordinary Wine window
geometry changes. They are not evidence that the composition bitmap was
resized. The test therefore did not exercise the code being tested.

Two X11 `BadWindow` reports remained, for resources `0x4800007` and
`0x3200008`. These indicate stale X11 window references, but the run does not
identify whether they affected the visible HTML surface.

### Decision

This is an inconclusive source result, not a fix. The bitmap change remains
isolated in the custom runner so it can receive a direct manual check. We must
not claim that it fixed the SOA flicker until a run changes the actual DComp
frame size and the owner confirms the screen remains correct.

### Next discriminating test

Use the current custom runner for one manual SOA session. Confirm whether the
embedded form is visible and whether the complete HTML panel still flickers.
If the game is resized during that session, inspect the trace for a
`Resized composition surface` line and compare the visual result. If the form
still flickers without a resize, the next investigation is the X11 stale
window reference and cross-process child lifetime path, not another bitmap or
configuration change.

## Bitmap size manual validation: 2026-09-13

### Expected behavior

The bitmap-size change should prevent a composition frame from being rejected
when the WebView swapchain changes dimensions. It should not otherwise alter
which window hosts the HTML surface.

### Observed result

The owner manually checked the embedded SOA form in interactive run
`20260911-195545-game` and reported that the complete HTML panel still
flickered with no visible improvement. The separate backup login window was
not used as the success criterion.

The run's trace contained many successful publications, but it did not contain
an actual DirectComposition frame-size change or a bitmap resize trace. The
manual result therefore rejects the change as a practical fix, while the trace
does not show that the new resize path was exercised.

### New evidence from the same run

The main target was created as a visible `Intermediate D3D Window`:

```text
create_target target window 0004068C ... parent 00010688 ... style 0x5c000000 ... visible 0.
show_window hwnd=0x4068c, cmd=5, was_visible 0
```

Later, the target was assigned parent `00010624`. The parent was a hidden
Chrome host with style `0x46000000`, which does not contain the `WS_VISIBLE`
bit:

```text
NtUserSetParent (0x4068c 0x10624)
target_update_surface composition surface parent 00010624 root 002E011E valid 1 visible 0.
do_composite composite target 0004068C destination 00010624 visible 0 offset 0,0 size 1258x740 copied 1.
```

In the current bridge, `copied 1` means the source frame was copied, not that
it reached the screen. Because the destination was reported invisible,
`target_update_surface` hid the temporary layered surface and returned before
publishing that frame.

This is strong evidence for a wrong destination choice in the bridge. It does
not yet prove that every visible SOA flicker comes from this target.

### Decision

The bitmap-size change is rejected as the SOA fix. It remains in the custom
runner only until the next source test is complete, then it must be either
kept as a separate correctness fix or removed if it is not useful.

### Next single source test

Change only `dlls/dcomp/device.c` so a DirectComposition target publishes to
its own target window rather than automatically selecting its immediate
parent. The target window is the HWND supplied to
`CreateTargetForHwnd`, so this follows the API object that is actually being
composed. The test will keep the existing bitmap, X11, and z-order behavior.

Success means the main target is no longer rejected as invisible while its
hidden Chrome host remains its Win32 parent. Failure means the target itself
also needs a separate visibility or X11-parent investigation. Those two
changes must not be combined.

## DirectComposition visibility-chain diagnostic: 2026-09-13

### Expected behavior

The embedded HTML target should remain visible because its top-level
DreamSeeker window is visible. Intermediate browser windows may exist in the
Win32 ownership chain, but they must not cause the presentation surface to be
hidden when the actual game window is on screen.

### Observed behavior

The owner manually checked the current build and reported that the embedded
HTML panel still flickers. Therefore the target-window change did not fix the
SOA symptom.

The logging-only run `20260913-214536-game` produced two different kinds of
chains. The main-sized `1258x740` target had visible Win32 ancestors all the
way to the visible DreamSeeker window and reached `UpdateLayeredWindow`.
Other browser targets had hidden ancestors. The run still reported four X11
`BadWindow` errors.

The important consequence is that a visible Win32 chain and a successful
publish call do not guarantee a stable image on the desktop. The diagnostic
also shows that the earlier explanation, which blamed one hidden Chrome
parent for every frame, was incomplete.

### Current model

The bridge can copy browser pixels into its intermediate bitmap and can call
the Windows layered-window presentation API. The remaining failure is likely
after that point, in the lifetime or mapping of the X11 child window, or in
how several browser surfaces are presented in sequence. This is a hypothesis,
not a proven root cause.

### Decision

The bitmap-size change and the target-window destination change are both
rejected as complete SOA fixes. The visibility-chain logging is retained as
evidence only. No further destination, z-order, or configuration change is
being treated as a fix without manual confirmation.

### Next discriminating test

Instrument the X11 presentation path only. Record the synthetic surface HWND,
its X11 whole-window handle, its X11 parent, whether it is mapped, and every
map, unmap, reparent, and destroy operation affecting it. Then compare those
events with the timestamps of the visible and missing HTML frames. A repeated
unmap, reparent, or use of a destroyed X11 handle would support the lifetime
hypothesis. If the surface stays mapped and valid while the image still
flickers, the next investigation must move to the frame-copy and paint
synchronization path.

## DirectComposition target-window experiment: 2026-09-13

### Expected behavior

The HWND supplied to `CreateTargetForHwnd` identifies the client area that
receives the DirectComposition visual tree. Publishing the temporary surface
under that target should avoid choosing a hidden browser parent as the
destination.

### Source change

The change was limited to `dlls/dcomp/device.c`. The bridge stopped replacing
the target HWND with `GetParent(target)`, and it reset the position offset to
zero. The existing bitmap-size change, X11 child handling, visibility check,
and z-order code were otherwise unchanged.

The previous installed bitmap-build DLL was saved as
`/tmp/dcomp-install-before-target-window-20260913.dll`.

### Automated result

Run `20260913-214118-game` completed with the application still running. It
recorded no error 87 and no failed composition-surface creation, but it also
recorded no successful `UpdateLayeredWindow` publication for the main HTML
surface. The relevant trace was:

```text
create_target target window 00020664 ... style 0x5c000000 ...
target_update_surface composition surface parent 00020664 ... visible 0.
do_composite composite target 00020664 destination 00020664 visible 0 ... copied 1.
```

The source pixels were copied into the DirectComposition intermediate bitmap,
but the target was still considered invisible. Four X11 `BadWindow` reports
also remained. The test therefore avoided one error by suppressing the
publication path, not by presenting the HTML panel.

### Decision

Using the target HWND alone is rejected as a complete fix. The evidence shows
that the target's own visibility result depends on its hidden ancestor, so the
destination and visibility decisions cannot be fixed by changing only one
handle.

The owner should not treat the absence of error 87 as improvement. A valid
result requires the real embedded form to be visible and stable on screen.

### Current model

The bridge currently has two incompatible pieces of information:

1. The DirectComposition target is the window that receives the browser's
   visual tree.
2. The X11 surface must be attached somewhere that is actually mapped and
   visible on the desktop.

The target and its browser host can be hidden Win32 intermediates even while
the SOA top-level window is visible. The bridge needs a generic way to map a
composition target to a valid presentation host while preserving the target's
client coordinates. The current ad hoc parent and target choices do not
provide that mapping.

### Next discriminating test

Add logging only for the complete target chain: target HWND, each Win32
ancestor, each `WS_VISIBLE` bit, each `IsWindowVisible` result, and each X11
whole-window handle. This will identify the first hidden ancestor and whether
the corresponding X11 window is mapped. No new destination or visibility
behavior should be changed until that chain is captured for the actual
`1258x740` embedded form.

## Regression in the target-window diagnostic build: 2026-09-13

### Expected behavior

The embedded HTML panel must be visible over the map. A diagnostic build that
only adds logging must not make the panel disappear.

### Observed behavior

The owner reported that the latest run showed only the SOA map. The earlier
bitmap-build run at least displayed the HTML panel, although it flickered.
The latest run therefore moves backward visually and cannot be used as a
candidate fix.

The log shows that the `1258x740` synthetic surface was mapped and reported as
`IsViewable` under X11. It does not show a successful DirectComposition
publication for the main target in that run. Three `BadWindow` errors were
also reported. This separates window existence from visible pixels, but does
not identify which operation caused the missing panel.

### Decision

Restore the last known visually better bitmap build before the next source
change. Preserve the new log as evidence, but remove the target-window
destination behavior and the synchronous X11 state queries from the active
runner. The rollback is a safety step, not a new experiment.

### Current model

The latest build may have changed timing because the logging helper performs
round-trip X11 queries. It also still contained the rejected target-window
destination behavior. Either can explain why the panel disappeared, so the
latest screenshot cannot be used to choose between stacking, mapping, and
pixel-copy hypotheses.

### Next discriminating test

Run the restored bitmap build with no diagnostic queries and confirm that the
panel returns to its previous visible-but-flickering state. Only then add
nonblocking request logging in the X11 path. If the panel remains absent after
the rollback, the regression is in an earlier shared source change and the
runner must be restored to the saved working checkpoint before proceeding.

## X11 stacking after mapping experiment: 2026-09-13

### Expected behavior

After Wine preserves the WebView2 document window and maps it inside the
DreamSeeker host, the browser surface should occupy the same child-window
position that Win32 selected. The HTML panel should remain above the D3D9 map
and should not disappear when the pointer moves over it.

### Observed behavior

The restored bitmap baseline again shows the real HTML panel, but the owner
reports that the panel flickers. This is the visual baseline for this test.

### Change under test

In `dlls/winex11.drv/window.c`, after `window_set_wm_state` requests the
preserved child transition from hidden to normal, call
`sync_child_window_z_order` once. The helper compares the Win32 previous
sibling with the X11 sibling and sends either an X11 sibling-order request or
an X11 raise request.

This changes only X11 stacking at the map transition. It does not change the
DirectComposition destination, the shared texture path, the WebView2 runtime,
the prefix, or the D3D9 and D3D11 selection.

### Why this could matter

`XReparentWindow` attaches the preserved browser surface to the DreamSeeker
X11 window. Reparenting can give the child a default X11 stacking position.
The Win32 sibling order and the X11 sibling order can therefore disagree.
The existing code already maps the preserved surface with `XMapRaised`, but a
later map or reparent request can still leave the X11 order different from the
Win32 order. The new call sends the order request after the map request.

This is a hypothesis, not a proven root cause. A successful run must show the
HTML panel over the map and must pass manual visual checking. A build that
merely stops flickering while hiding the panel is a regression, not a fix.

### Next discriminating test

Build only the X11 driver, run the same isolated SOA prefix, and manually
check whether the panel is visible and stable. If visibility is unchanged but
flicker remains, X11 sibling order is not the only problem. If the panel
becomes stable and stays above the map, compare the X11 trace for the new
restack request before considering any further source change.

### Result

The owner manually tested run `20260913-224252-game` and saw the same HTML
panel flicker. The X11 stacking change therefore produced no visible fix.

The log cannot prove whether the new restack call ran. The test used
`warn+x11drv`, while the relevant messages in `sync_child_window_z_order` use
the `TRACE` level. The log contains no restack message because that level was
not enabled. It also contains no textual `BadWindow` report.

There is one nonconclusive difference from the restored baseline. The baseline
log contains one warning that a DirectComposition parent was no longer a
window. The restack run does not contain that warning. However, both runs
ended after the same automatic wait, and the restack run produced far more
`dxgi_surface_GetDC` messages. The runs therefore did not produce a clean
performance comparison, and the warning difference cannot be credited to the
stacking change.

### Decision

The post-map restack change is not a fix. The X11 stacking hypothesis remains
unproven because the manual result is negative and the diagnostic level was
too low to confirm the operation. The next single test should enable
`trace+x11drv` for the same source and prefix. That will show whether the
preserved browser surface receives the expected X11 sibling or raise request.

## X11 trace result and coordinate mismatch: 2026-09-14

### Expected behavior

The preserved HTML surface should be mapped inside the DreamSeeker host at the
Win32 position and size selected by WebView2. After that, the X11 position and
size should remain consistent with the Win32 state while the browser publishes
new pixels.

### Observed behavior

The owner manually tested run `20260914-122642-game` and reported that the
HTML panel still flickers.

The trace proves that the stacking call executed for the likely main HTML
surface. Wine logged the following sequence for X11 window `320000b`:

1. It preserved the window and attached it under X11 parent `2a00006`.
2. It requested `XMapRaised`.
3. It called `sync_child_window_z_order` with `preserved 1`.
4. It raised the surface above its child siblings.

The same run then reported repeated geometry mismatches. For example, Wine
requested `(0,0)-(1258,740)`, while X11 reported `(0,28)-(1258,768)`. It
also reported the same pattern for a `1358x840` request, with X11 returning
`1358x868`.

### Current model

The stacking operation is reaching X11, so a missing initial raise is not
enough to explain the flicker.

The stronger hypothesis is that the preserved browser surface mixes two
coordinate systems. Its new X11 child configuration is sent using coordinates
relative to the DreamSeeker parent. In `X11DRV_ConfigureNotify`, the incoming
child coordinates are then passed through `host_window_map_point`, which maps
them back toward root coordinates. The state comparison can therefore see a
parent-relative Win32 rectangle as a root-relative X11 rectangle. The extra
28 pixels match the top-level host frame reported by X11 in this run.

This is strongly suggested by the trace, but it is not yet a proven cause of
the visible flicker. The mismatch may trigger repeated state updates, but the
trace does not by itself prove that each visual flicker corresponds to one of
those updates.

### Next discriminating test

Use one source change in `X11DRV_ConfigureNotify` for preserved children. Keep
their configure position in the parent coordinate system instead of converting
it through the root mapping path. Remove the post-map stacking change while
doing this, so the next run differs from the confirmed baseline in only this
coordinate conversion behavior. Then check whether the HTML panel remains
visible and whether the `mismatch config` warnings disappear.

## Parent-relative configure test result: 2026-09-14

### Expected behavior

The coordinate change should remove the false X11 geometry mismatch while
leaving the HTML pixels visible over the D3D9 map.

### Observed behavior

The owner reported that the flicker disappeared because the HTML panel was no
longer visible. This is a visual regression, not a fix.

The trace shows that the likely main HTML surface `0x10640/320000b` was still
mapped with `XMapRaised`. Its configure events now matched the requested
parent-relative rectangles exactly. The run contained no `mismatch config`
message for that surface. It also contained no unmap message for that surface.

The run did not enable DirectComposition trace logging. It therefore contains
no evidence showing whether `target_update_surface`, `do_composite`, or the
X11 pixel publication path copied the HTML surface. The same six
`device_QueryInterface` and six `visual_SetContent` fixme messages appeared in
both the stacking run and this coordinate run, so they cannot be attributed to
the coordinate change.

### Decision

The coordinate change fixes a real bookkeeping mismatch, but it does not by
itself present the HTML panel. Keep it as diagnostic evidence, not as a
candidate fix. The missing panel may be below the map, or the DirectComposition
surface may not be reaching X11. The current log cannot distinguish those two
cases.

### Next discriminating test

Run the coordinate build once with `trace+dcomp` added to `WINEDEBUG`. Inspect
whether the main composition target receives pixels and whether its parent is
considered visible. This is a logging-only test. It must not change the X11
source or the prefix.

## DirectComposition trace and X11 child order: 2026-09-14

### Expected behavior

The browser surface should be copied into its preserved HTML window, and that
HTML window should be above the D3D9 map in the DreamSeeker host.

### Observed behavior

Run `20260914-130034-game` used the parent-relative coordinate build with
`trace+dcomp`. The main DirectComposition target repeatedly logged
`do_composite` with destination `0001063C` and `copied 1`. Later entries showed
that destination as visible. The source code confirms that this path copies
the WebView2 swapchain into a Wine layered child and publishes it with
`UpdateLayeredWindow`.

The live X11 tree showed the following relevant windows under host `2a00006`:

1. HTML document window `320000b`, size `1258x740`.
2. Full-size map window `3200008`, size `1366x740`.

The X11 composition child under the HTML window was also viewable. It was
window `3e00009`, size `1258x740`, with map state `IsViewable`.

The child list by itself is not treated as proof of the final visible order.
The important fact is that both windows existed with the requested geometry,
and the HTML window had a viewable composition child. A separate raise test
was needed to determine whether changing the X11 order changed the visible
result.

### Current model

This run rules out the simple explanation that WebView2 failed to create a
surface. Wine received the browser swapchain, copied it, created a viewable
composition child, and kept the HTML document window mapped. The remaining
failure may involve the final sibling order of the preserved HTML window and
the D3D9 map, or a later operation that changes the order. The child list alone
does not identify which window is visually on top.

This is stronger evidence than the earlier trace, but it is still not proof
that sibling order is the only problem. A later isolated raise run is recorded
below.

### Decision

Do not change the DirectComposition or prefix configuration based on this run.
The pixel publication path is active. The temporary raise check must be
repeated while the test is open before changing the X11 source again.

### Next discriminating test

Launch the same build with no source or prefix changes. While the HTML panel is
absent, raise only X11 window `320000b`. If the panel appears, the test proves
that the remaining failure is sibling stacking. If it does not appear, inspect
the published child pixels directly before attempting another stacking change.

## Temporary X11 raise test: 2026-09-14

### Expected behavior

If the HTML window is hidden only because of its sibling order, raising that
one window should make the normal HTML panel appear above the map and leave
the map visible through the transparent parts of the browser surface.

### Observed behavior

Run `20260914-131053-game` used the same source and prefix as the trace run.
Before the test, the host tree contained the full HTML window `320000b` and
the map window `3200008`. The composition child under the HTML window was
`3e00009` and was viewable.

An isolated `XRaiseWindow` call was sent only to `320000b`. The next tree query
showed that `320000b` moved to the first position in the host child list.
The captured host window then showed the form faintly, but most of the host
was covered by a dark surface. The map was visible only outside the HTML
window's rectangle. This is not the expected transparent composition.

### Current model

The raise changed the X11 relationship, so window order is involved in the
failure. It did not produce a usable result. The dark covering surface also
shows that the alpha or layered-window handoff is wrong after the HTML window
is brought forward. The current evidence does not justify changing only the
stacking code.

### Decision

Do not treat the temporary raise as a fix. Do not change the prefix. The test
rules out the idea that the browser content is entirely missing, but it does
not distinguish whether the dark surface comes from premultiplied alpha,
layered-window publication, or an X11 compositor limitation.

### Next discriminating test

Inspect the X11 layered surface created by `UpdateLayeredWindow` and compare
its alpha data with the source WebView2 bitmap. The next source experiment
should change only that pixel and alpha handoff. It should be tested with the
same DreamSeeker run and should be accepted only if the map remains visible,
the HTML panel is fully visible, and the result stays stable without a manual
raise.

## DComp pixel and alpha trace: 2026-09-14

### Expected behavior

Immediately before Wine publishes the layered surface, the DComp surface
should contain the browser pixels. Transparent parts should have zero alpha,
and opaque parts of the HTML panel should have alpha 255. The exact counts
depend on the page and its CSS.

### Change under test

The DComp target now keeps the address of its 32 bit DIB section and scans the
copied pixels after `BitBlt`. It logs counts for zero, partial, and opaque
alpha, the number of pixels with nonzero color, the maximum color value, and
the first pixel value. This is a logging-only change. It does not change the
X11 stacking code, the prefix, the blend flags, or the published pixels.

The installed DLL was backed up as
`/tmp/dcomp-install-before-pixel-stats-20260914.dll`. The attempted make
install target did not exist, so only the rebuilt DComp DLL was copied into
the isolated custom Wine installation.

### Current model

The previous run proved that changing the X11 order is not sufficient. It
showed a dark covering surface with faint browser content. We still do not
know whether those dark pixels are already present in the DComp DIB or are
introduced while Wine turns that DIB into an X11 visible surface.

### Next discriminating test

Run the same isolated SOA session with `trace+dcomp`. Meaningful browser color
values and a sensible alpha distribution would move the investigation past
`BitBlt`, toward `UpdateLayeredWindow` and X11. Nearly black colors or an
unexpected alpha distribution would locate the problem before X11. The manual
screen result remains the acceptance test.

## Protect composed child windows during map presentation: 2026-10-14

### Confirmed baseline failure

The owner confirms that the login panel still flickers with the September 26
buffer selection change. During baseline run `20261014-003758-game`, the owner
also reported that the panel sometimes disappears for long periods. Neither
the unchanged frame cache nor the buffer zero selection passed manual acceptance.

The expected behavior is a stable HTML panel above a continuously rendered map.
The baseline still uses the isolated test prefix and the existing custom Wine
11.15 build. Browser arguments and prefix settings were not changed for this
comparison. No footage was recorded.

### Measurements

Read small pixel samples from X11 window storage and query the native child
order. These measurements examine the rendering pipeline. They do not replace
the owner's visual acceptance test.

* All 120 samples of the browser composition window contained the same panel
  colors, including an opaque pixel `0xff381c02`. A sample outside the panel
  remained transparent. The host child order was unchanged across all samples.
* A second measurement read the final game hierarchy's redirected pixmap and
  the map's separate redirected source. At the corresponding opaque panel
  position, all 180 final game samples were `0xededed`, equal to the map source.
  The browser window still contained `0xff381c02` at that position.
* A wider grid contained map colors in both the final game pixmap and the map
  source, while the independent browser surface contained the panel.

The browser surface therefore contained panel pixels while those pixels were
absent from the final game image. This localizes this observation after browser
rendering and before final display. It does not prove that every historical
flicker had the same cause.

### Code mechanism and correction under test

`X11DRV_client_surface_present` copied the offscreen map to its host using
`IncludeInferiors`. That X11 mode permits drawing through child windows.
`ClipByChildren` instead protects viewable child regions, as described in the
[X11 protocol](https://www.x.org/releases/X11R7.7/doc/xproto/x11protocol.html).
The [Composite protocol, sections 3.1 and 3.3](https://sources.debian.org/src/xorgproto/2018.4-4/compositeproto.txt/)
also explains clipping around redirected children.

The single presentation correction touches three files in the existing Wine
source tree:

1. `dlls/winex11.drv/init.c` selects `ClipByChildren` for the final offscreen
   client copy. Independently drawn child windows must survive that copy.
2. `dlls/winex11.drv/xrender.c` uses the graphics context's requested child
   clipping mode when creating destination and source XRender pictures. It
   previously forced `IncludeInferiors`, overriding the mode chosen by callers.
3. `dlls/winex11.drv/window.c` limits the earlier native window preservation
   experiment to layered windows. Ordinary browser children return to Wine's
   normal reparenting behavior. Retaining full size opaque native browser
   windows would otherwise prevent map updates across their rectangles when
   child clipping is respected.

This correction contains no game title checks, panel coordinates, periodic
raising, or forced redraw animation. The other experimental Wine changes are
still present and have not been committed.

The source files before this correction are saved in
`/tmp/soa-before-child-clipping-20261014.tar.gz`. The installed native driver
before the correction is saved as
`/tmp/soa-winex11-before-child-clipping-20261014.so`.
Both backups are also preserved together in
`test-results/safety/soa-before-child-clipping-20261014.tar.gz`, so the rollback
checkpoint does not depend on temporary files surviving a restart.

### Validation status

The native X11 driver compiled successfully and `git diff --check` passed.
Candidate run `20261014-004836-game` uses the rebuilt driver in the same isolated
prefix. The installed driver checksum matches the newly built driver, and the
DreamSeeker process maps that installed file.

The candidate changes the final composed image. All 120 opaque panel samples
now match the independent browser surface. All 120 outside samples match the
map source. In the baseline, none of the 180 corresponding panel samples
matched the browser.

A wider grid comparison across twelve frames found no mismatches in 20,040
opaque samples or 150,312 transparent samples. Opaque final pixels were checked
against the browser surface. Pixels where that surface had zero alpha were
checked against the map source at the same host coordinates. This grid did not
contain partial alpha samples, so it does not validate that case.

A subsequent 60.09 second check sampled the live combined image 419 times.
All 419 panel samples matched the browser, and all 419 outside samples matched
the map. There were no X11 errors or sampled disappearances. The game remains
open for the owner's visual and interaction checks.

### Manual acceptance and recovery commit

The owner confirmed that the embedded panel is stable and usable. The supplied
recording `Screencast_20261014_101521.mp4` shows successful login and stable
character selection panels. This is the accepted recovery baseline, rather
than a claim that the entire rendering implementation is finished.

The complete Wine source state is committed on branch `dcomp-composition`
in the personal Wine fork. The original Wine
repository remains the `origin` remote. The personal fork is remote `personal`.

Remaining issues reported by the owner and visible in the recording are thick
black borders around panels, brief white panel flashes, and severe performance
overhead, reportedly about one frame per second. The recording contains white
flashes during character selection as well as the black borders. Preserve this
commit while addressing those issues in subsequent changes.

### Follow-up on borders, flashes, and performance

The supplied recording lasts 9.86 seconds. It shows usable character selection
cards with thick black rounded borders and brief white card and strip flashes.
These are separate acceptance failures after the stable-panel checkpoint.

The compositor contained an unconditional diagnostic that downloaded both
swapchain buffers. Its counter was not advanced on the successful direct path,
so disabling trace output did not stop that work. The candidate removes the
downloads, reuses the staging texture and CPU image buffer, and skips unchanged
immutable browser frames. Mutable fallback buffers are not treated as immutable.
Hidden targets also skip composition work. Cached resources are released when
content changes and when their visual is destroyed.

Comparative ten-second process samples showed the browser GPU process using
about 91 percent of one CPU before cleanup and about 38 percent after caching.
DreamSeeker CPU use rose as more CPU time became available. A separate QEMU
process consumed several CPU cores during these tests and was not modified.
These measurements do not establish in-game performance after login. Wine's
map presentation counter reported roughly 16 to 22 frames per second at login.

The browser supplies partial alpha, meaning partially transparent pixels, in
its actual image. In run `20261014-105719-game`, a native upload retained a
center pixel of `04 0d 1a df` in memory. The corresponding native window pixel
was `04 0d 1a ff`, where `ff` means fully opaque. A temporary one-pixel test,
restored immediately afterward, confirmed that a core X11 window image write
made alpha opaque while an XRender source copy preserved it. An independent
32-bit pixmap also retained alpha correctly.

An alpha-preserving upload candidate in run `20261014-110528-game` retained
108,788 partially transparent pixels in the browser's native backing image.
However, final-image checking found all 12,102 sampled partial-alpha pixels
copied without blending over the map. Only 1,343 matched the expected blended
color within two channel levels, including coincidental matches on dark map
pixels. This candidate alone therefore does not fix the black border.

The next source change tests explicit child-image blending after presenting
the offscreen map, with native child images redirected rather than copied
automatically. Its result must be checked against the final combined image,
not merely the browser backing image. A separate first-frame change creates
composition windows hidden and shows them only after publishing their image.
Ordinary GDI paints and cached window-surface flushes also use child clipping
so they do not overwrite independent child content. White-flash behavior after
login still requires the owner's manual verification.

All follow-up changes remain separate from the pushed recovery commit. No
successful build, pixel sample, or frame counter replaces manual acceptance.

The first explicit-blending run, `20261014-111112-game`, matched all 12,102
partial-alpha samples in an initial frame check. A broader twelve-frame check
then found 155,880 mismatches in 156,288 transparent-background samples, despite
zero mismatches in 20,040 partial-alpha samples. This rejected that candidate.
XRender source reads did not apply the native child's bounding shape, so
opaque backing pixels outside the panel could cover the map. The correction
applies that bounding shape to the destination clipping region before blending
each child and also intersects it with the existing visible-region clip.

Corrected run `20261014-111521-game` uses the rebuilt driver and compositor.
It does not change the prefix configuration, game, HTML, or Chromium options.
Temporary alpha scans and warning diagnostics are removed. Expensive legacy
pixel diagnostics now require trace output to be enabled. The run only logs
errors and the existing frame-rate counter, with no live footage capture.

That corrected run preserved all 156,288 transparent-background samples. It
still had 1,538 mismatches among 20,040 partial-alpha samples across twelve
frames. A subsequent single-frame check had at most one channel level of
error in all 12,102 partial-alpha samples. The intermittent failure exposed
the map between its presentation and the separate child-blending operation.
The follow-up candidate draws the map and blends the children into a reusable
offscreen image, then publishes the complete image in one native copy. This
avoids exposing the intermediate map-only frame.

### Completed-frame candidate validation

Run `20261014-112028-game` uses completed-frame publication with child-shape
clipping and alpha-preserving uploads. The native driver and compositor both
compiled successfully, and `git diff --check` passed. The installed native
driver checksum matches the newly built driver.

A 31.46 second check took 36 synchronized samples of the final combined game
image, the browser image, and the independent map image. No images or footage
were saved. All 468,864 transparent-background samples matched the map. All
60,120 partial-alpha samples matched the expected premultiplied-alpha blend
within two levels per color channel. There were zero mismatches in either
category. This is stronger than the earlier checks, which missed partial
alpha or checked the browser image without checking final presentation.

The actual map presentation counter remains roughly 16 to 21 frames per second
at login under the current system load. This is not a measurement of movement
or interaction speed after login. The game remains open for manual verification
of the borders, white flashes, usability, and in-game performance.

Follow-up source changes are uncommitted pending that verification. The
accepted recovery checkpoint remains pushed as `a6b9503d432df017e25b71940dd6755840ef097f`.
The full Wine test suite has not been run for this candidate. Broader window
lifecycle, clipping, multiple map-view, and performance regression testing
remains necessary before proposing these changes for inclusion in official Wine.

### Second accepted recovery checkpoint

The owner manually confirmed that the black borders are gone and panels no
longer flash white. In-game performance is slightly better but remains very
poor and resource intensive. The same computer ran the game smoothly under
Windows 11. Visual correctness is accepted, not performance completeness.

This state is committed and pushed on `dcomp-composition`. Subsequent performance work must preserve this recovery
point and the accepted visual behavior.

The final log from the accepted run also contains an X11 `BadWindow` error on
`X_DestroyWindow` for native window `0x4800017`. This is a separate lifecycle
failure to track. It does not prove the cause of the in-game slowdown.

### Performance pass after the accepted visual checkpoint

Baseline run `20261014-131409-game` keeps the accepted source and prefix
configuration unchanged. Hardware inspection reports accelerated Intel UHD
Graphics 620 rendering with Mesa 25.0.7, rather than a software renderer.

The system is also under substantial memory and CPU pressure. The initial
inspection showed 7.6 GiB RAM, about 1.0 GiB available, and 4.3 GiB in swap.
Two one-second samples showed roughly 375 to 385 MiB per second of swap output
and about three percent idle CPU time. A separate QEMU process used roughly
341 percent CPU in an eight-second sample and had about 2.3 GiB resident plus
2.0 GiB swapped memory. That process was not stopped or changed. This is a
measurement confounder, not a reason to dismiss Wine's own overhead.

The same eight-second sample measured DreamSeeker at 92 percent of one CPU,
Wine's server at 40 percent, and Chromium's GPU process at 36 percent. A later
five-second thread sample measured the DreamSeeker main thread at 65 percent
and its graphics worker at 14 percent. Debugger samples place the main thread
inside BYOND's own core module. Native stack symbols were loaded explicitly
because the Wine preloader did not expose normal shared-library tracking to
the debugger. Frame arguments were suppressed to avoid reading game input.

A twelve-call debugger timing of `X11DRV_client_surface_present` measured
4.29 to 28.07 milliseconds, with a median of 13.21 milliseconds. Debugger probes
add overhead, so this is an approximate presentation-path measurement, not an
unbiased in-game frame-rate benchmark. Both the source and destination map
rectangles were 1366 by 740 at origin zero.

The first performance-only candidate removes the extra `XSync` following each
`XGetWindowAttributes` reply and the forced synchronization after child
blending. Attribute queries already wait for replies. Render commands and the
final completed-frame copy remain ordered on the same connection. Existing GDI
error handling covers concurrent child destruction. Redirection setup and
teardown retain their explicit error checks. Blending, clipping, cached images,
and completed-frame publication are unchanged.

Only `dlls/winex11.drv/xrender.c` changes in this candidate. It compiles and
passes `git diff --check`. Run `20261014-132131-game` uses the candidate with
the same prefix and browser options. The exact accepted native driver is also
saved as `/tmp/soa-winex11-057a04bc.so` for immediate binary recovery. The
candidate is not committed pending measurement and manual verification.

Repeating the twelve-call timing on the candidate measured 3.51 to 27.73
milliseconds, with a median of 15.24 milliseconds. This does not demonstrate
a speedup over the 13.21 millisecond baseline median under changing load.
It must not be reported as a successful performance fix.

A twelve-frame image check found no mismatches in 20,040 partial-alpha samples
and 23 mismatches in 156,288 background samples. The background mismatches have
not been diagnosed and are not a basis for claiming complete visual equivalence.
The candidate remains uncommitted and available for manual evaluation.

Further process inspection identifies the QEMU guest as `byond-win11`, assigned
4 GiB RAM and four virtual CPUs. It started on October 12 at 19:19 local time.
This is directly relevant to the memory-pressure investigation. The owner was
asked for permission to shut it down cleanly before the next benchmark because
it may contain unsaved work. No shutdown, pause, or guest modification has been
performed. Wine's own overhead still requires investigation after controlling
this competing workload.

### Graceful VM shutdown and logged-in scene measurements

The owner left SOA logged in to a slow scene and reported slideshow movement
and inventory opening delays of about thirty seconds. The owner explicitly
approved a clean shutdown of `byond-win11`.

The exact libvirt guest was resolved as `byond-win11`, UUID
`f8a5df55-0bc2-4d18-830e-eb41f0a04ea9`. A normal `virsh shutdown` request was
sent. The lifecycle event reported `Shutdown Finished after guest request`,
and guest state became `shut off (shutdown)`. No forced shutdown or guest disk
deletion was used. The SOA process was not restarted or modified.

Immediately before shutdown, the machine had about 824 MiB available memory
and almost all of its 7.9 GiB swap area in use. After shutdown, available memory
rose to 3.7 GiB. Swap allocation remained high because inactive swapped pages
need not be read back immediately. Subsequent one-second samples showed no
swap output and only small swap input, unlike the earlier heavy swap traffic.
The same game process's presentation counter changed from approximately
11 to 13 frames per second immediately before shutdown to approximately
19 to 21 afterward. This is an environmental improvement, not a Wine patch
result. The owner was asked to retry movement and inventory in the same scene.

Read-only browser inspection found sixteen page targets. A five-second browser
performance-counter sample found most pages nearly idle. Two pages used about
198 and 128 milliseconds of browser task time per second. Their JavaScript
execution measured about 1.9 and 23.8 milliseconds per second respectively.
One page spent about 61 milliseconds per second recalculating styles. Browser
performance measurement was disabled afterward. Form contents were not read.

An eight-second CPU sample after VM shutdown measured the browser graphics
main thread at 96.5 percent of one CPU and its Direct3D worker at 75.1 percent.
The composition worker used 17.1 percent, and Chromium's visual compositor
thread used 15.1 percent. DreamSeeker's main thread used 87.5 percent and its
graphics worker 24.4 percent. Resource contention was not the only problem.

Debugger probes of the graphics path recorded 525 calls to
`d3d11_swapchain_present`, with a median observed duration of 5.475 milliseconds
and maximum of 30.118 milliseconds. These timings include debugger overhead.
No `composite_frame` or `target_present_surface_frame` completions were captured
during that interval. A separate fifty-call classification confirmed that all
fifty calls used composition swapchains, rather than ordinary window swapchains.

A sixty-iteration composition-target sample found sixteen targets, of which
four had `surface_visible` set. A final matched sample walked each target's
visual tree and associated its swapchain content with producer calls. All
eighty sampled producer calls belonged to swapchain `0x00c77c48`, associated
with target `0x30606`, whose `surface_visible` flag was zero. Thus the sample
demonstrates repeated frame production for an undisplayed composition target.
It does not by itself prove the complete cause of the inventory delay.

Source inspection confirms that every composition swapchain present currently
creates a fresh graphics texture, copies the back buffer into it, flushes the
graphics context, and publishes that immutable snapshot. This work happens
even if no visible target consumes the changing frames. The next optimization
should preserve immutable completed-frame ownership while avoiding repeated
allocation and unnecessary producer work. Reusing an image that a visible
consumer still holds, discarding the latest hidden frame, or reporting false
occlusion would risk reintroducing missing panels and must not be used as a
shortcut. No additional Wine source change was made during this measurement.

### Power profile and remaining panel latency

After the VM shutdown, the owner confirmed smoother movement, but performance
remains worse than Windows 11 on this laptop. Inventory, skill tree, and other
WebView2 panels still take a very long time to open. The environmental
improvement must not be described as a Wine source improvement.

Read-only power checks report the active profile as `balanced`, with AC power
connected. The CPU uses `intel_pstate`, the energy preference is
`balance_performance`, and `no_turbo` is zero. The governor string `powersave`
alone does not establish that the Power Saver profile is selected. No power
setting was changed. These readings do not exclude thermal throttling or prove
that a different profile would make no difference.

A brief debugger inspection of the active browser graphics worker confirmed
the Vulkan rendering path. Its selected adapter description reports
`Intel(R) UHD Graphics 620`. The process maps both Intel and software Vulkan
driver libraries, which is not evidence by itself that software rendering was
selected. The general OpenGL hardware report from the earlier inspection was
not sufficient to identify this browser's active graphics path.

A forty-frame staged debugger timing of the composition snapshot producer
measured median durations of 1.489 milliseconds for texture allocation,
0.874 milliseconds for the copy and flush section, and 0.704 milliseconds
for snapshot publication. The corresponding maxima were 5.499, 3.329, and
4.009 milliseconds. Thirty-seven snapshots were 1258 by 740 and three were
234 by 124. These are debugger-instrumented wall times, not unbiased CPU costs,
and the stages do not cover the entire presentation function.

A separate 120-call presentation sample measured approximately 19.46 calls
per second over 6.114 seconds. Every sampled call requested sync interval zero
and flags zero. This sample therefore does not support blaming an ignored
nonzero sync interval for the current workload, or adding an arbitrary frame
rate limit as a fix.

Read-only browser inspection then found nineteen page targets. A ten-second
performance-counter sample again found most pages almost idle. The two active
1258 by 740 pages used approximately 206 and 124 milliseconds of task time per
second, with approximately 2 and 21 milliseconds of JavaScript respectively.
The other pages each used under 8 milliseconds of task time per second during
this sample. Document queries reported completed loading, but that does not
prove when a newly requested panel became visible. Browser counters do not
include all graphics processing or the game's request handling. Measurement
was disabled and browser connections closed afterward.

Expected behavior is prompt opening of an embedded panel with usable movement.
Observed behavior is improved movement after removing VM load, but persistently
slow panel opening. The current model has proven redundant snapshot allocation
and unconsumed frame production, but neither finding explains the complete
reported opening delay yet. The next discriminating test is to correlate a
manual panel-open request, the composition update, and first frame publication.
That separates a delayed request from delayed drawing before choosing a source
optimization. Reusable snapshots would require explicit ownership so no image
is overwritten while a consumer still holds it.

SOA remained running in the logged-in scene. Debuggers detached after each
bounded probe. No additional source change, prefix change, recording, driver
installation, or power-profile change was made during this follow-up.

### Appearance-only font correction

The owner reported much better playability but remaining performance problems,
monochrome or simplified symbols, and different health-bar text. Font work is
classified as appearance work. No frame-rate or panel-latency improvement is
claimed from these changes.

SOA was no longer running when this inspection began. Read-only host checks
found no matching Wine processes, no SOA windows, and no debugging listener on
port 9242. The single test session was reopened as `20261014-185951-game`, with
the same Wine build, prefix, browser arguments, and graphics settings. The
owner logged in and opened settings. No screenshot or recording was captured.

Actual browser font inspection found:

- The 819 by 555 settings page requests Arial. Its rendered fonts were Arial,
  DejaVu Serif, Noto Sans Symbols, and Noto Sans Symbols2.
- Several other pages request Segoe UI and fall back to Arial or Tahoma.
- The large login document successfully loads the web fonts Cinzel and Noto
  Sans. Its unused face declarations are not evidence of failed downloads.
- No inspected browser document requested UberBit7. The cached native map
  control's stylesheet instead explicitly sets `statbar_maptext` and its
  supplies variant to UberBit7 at 12 pixels. This ties that font to the native
  health display, not to a guessed WebView2 font.

Inspection queried font names, styles, and rendered font metadata. It did not
read login inputs or print game chat. The initial body-only font query returned
empty results because text was nested in child elements. The subsequent
element-level query obtained actual rendered fonts. Empty body-only results
must not be mistaken for missing browser text.

A fresh DirectWrite font-collection probe could not find Calibri, Segoe UI,
Segoe UI Emoji, Twemoji Mozilla, UberBit7, Minimal4, or Ninja Naruto before
installation. The three SOA font files already existed in its resource cache,
but this does not establish whether DreamSeeker had separately registered them
as process-private fonts. Installing them into the test prefix ensures that
they are available to fresh system font collections too.

The probe also uncovered three missing regular-face registrations. The files
`arial.ttf`, `cour.ttf`, and `times.ttf` were present, but their regular font
registry values were absent. A normal Arial request selected `ariali.ttf`.
After restoring the registrations, fresh normal Arial, Courier New, and Times
New Roman requests resolve to their respective regular files.

The added font set contains thirteen files:

| Files | Source and role |
| --- | --- |
| Four Carlito faces | Debian `fonts-crosextra-carlito` version `20230309-2`. [Carlito](https://github.com/googlefonts/carlito) is metric-compatible with Calibri and uses the SIL Open Font License. |
| Five Selawik faces | [Microsoft Selawik release 1.01](https://github.com/microsoft/Selawik/releases/tag/1.01). Microsoft describes it as an open source Segoe UI replacement. It is not an exact visual match and has documented hinting and kerning limitations. |
| `TwemojiMozilla.ttf` | Existing Firefox font at `/usr/lib/firefox-esr/fonts/TwemojiMozilla.ttf`, version 0.7.0. [Mozilla's project](https://github.com/mozilla/twemoji-colr) supplies layered color emoji. This is not Windows emoji artwork. |
| `UberBit7.ttf`, `Minimal4.ttf`, `Ninja Naruto.ttf` | Existing SOA resource cache. These files remain private runtime data and were not added to the project repository or cleared for redistribution. |

The Debian package SHA-256 is
`1f86d76afaef81e19ce3b425dff6d9523ffb186f1f0fd9e40292e39d81c28147`.
The Selawik release archive SHA-256 is
`3f62c51e05e3b5a1e6241cf92a371f0be2ea1183aa87b30718bbd40832a8d423`.
The existing Firefox font SHA-256 is
`6d90152ee0d29e82fe2a87793af5aa4b7ad13e6538360889e141e81ed299ee8e`.

All thirteen files were copied without overwriting existing files into the
test prefix's Windows Fonts directory. License notices for Carlito, Selawik,
and Twemoji were retained in its `drive_c/font-licenses` directory. No fonts
were installed system-wide or into the regular game prefix. No proprietary
Windows font download or font-file renaming was used.

`config/byond-fonts.reg` registers these files, restores the three missing
regular faces, and adds missing-family aliases for Calibri to Carlito, Segoe UI
to Selawik, and Segoe UI Emoji to Twemoji Mozilla. The aliases are configured
for both DirectWrite and GDI. Existing real families are not replaced by the
DirectWrite missing-family mapping. No Arial-wide emoji substitution or game
stylesheet modification was applied.

After registration, the fresh probe verifies the actual font-file paths for
all three aliases and the SOA fonts. Both Twemoji Mozilla and its Segoe UI
Emoji alias return success from `TranslateColorGlyphRun` and two color layers
for the test character U+1F30D. This proves a working color-font path in this
Wine build, not correct fallback or appearance in every real browser control.
Some characters may intentionally request text presentation. Coverage of the
actual missing characters has not been checked. The internal font version
0.7.0 does not establish its supported Unicode version. A running browser may
retain old font caches.
The owner was asked before restarting the logged-in session for visual testing.

The owner then approved the restart. A normal window-close request was sent to
the single test DreamSeeker process, and its runner reported `app_exited=1`.
Run `20261014-193241-game` reopens the same server with the same Wine build,
prefix, graphics settings, logging settings, and existing browser profile
`20261014-185951-game`. Only the log filename changes for the new launch. No
recording or screenshot collection is enabled. This restart reloads the font
configuration and is not a new composition or performance experiment. The
composition DLL checksum remains
`1de351f8fd34098b9d6e1dfa3fab887cc56d4accb2afe10810bba98b4499cdb5`.
The final in-game appearance still requires the owner's verification.

Recovery files are in
`test-results/safety/soa-fonts-before-20261014`, containing the prior Fonts and
FontSubstitutes registry exports. All values added by this experiment were
absent before it. `config/byond-fonts-undo.reg` removes only those values.
To fully undo the experiment, close the test session, import that undo file
with the same test-prefix Wine, and move only the thirteen added font files
from Windows Fonts into the safety directory. Leave all previously installed
core font files in place. The three SOA originals remain in the resource cache.
Restart the test session afterward. Removing registrations alone may leave
fonts discoverable from the font directory, so it is not a complete rollback.

### Performance options after the font inspection

Keep appearance acceptance separate from performance acceptance. No Wine source
was edited for the font correction. The earlier uncommitted XRender wait
cleanup remains the only source delta from checkpoint `057a04bc` and still has
no demonstrated speedup.

Outside the composition change:

1. Investigate CPU throttling without disabling protection. In a six-second
   read-only sample, the CPU 0 package counter rose from 15,441,681 to
   15,442,299, while its core counter remained 360,047. Sampled CPU 0 frequencies
   were about 1.85, 1.60, and 1.70 GHz. Package temperatures were 69 to 70
   degrees Celsius. The [kernel documentation](https://cdn.kernel.org/doc/html/latest/admin-guide/thermal/intel_thermal_throttle.html)
   describes these as thermal status counters. This confirms counter activity,
   not the precise trigger, fraction of time lost, or that changing a power
   profile will solve it. A controlled Balanced versus Performance comparison
   should also measure temperatures and throttle duration if available.
2. Keep memory pressure controlled. The separate Windows VM is shut down.
   During this inspection about 1.3 to 1.5 GiB memory remained available.
   Short samples showed no swap output. Already allocated swap is not itself
   evidence of current swapping. Avoid recording and other heavy workloads
   during the comparison.
3. Measure a clean play launch without browser verbose logging or Wine frame
   tracing against the diagnostic launch. The current logs are relatively
   small, so logging has not been established as the main slowdown. Reuse a
   test browser profile for warm-cache comparisons instead of silently
   comparing a fresh profile against a warmed Windows installation. Preserve
   the current graphics choices for these tests.
4. Investigate font enumeration as a separate possible panel-startup cost.
   Fresh isolated DirectWrite collection probes took 9.651, 10.382, and 9.366
   seconds after registration. The system reports 2,608 font faces. The probe
   explicitly requests a font-collection refresh, and it does not prove SOA
   performs the same operation during every panel opening. Correlate it with
   a real slow opening before changing font discovery or claiming causality.
   Do not remove the user's system fonts to test this.

The build already uses `-O2`. Debug symbols do not establish that the game is
running unoptimized code. The active browser device reports Intel UHD 620.
No reinstall or speculative graphics-flag sweep is justified by these checks.

Inside the composition change, the measured source work supports this order:

1. Time a real panel-open request, composition update, and first publication.
   Distinguish the reported opening delay from ordinary per-frame overhead.
2. Cache X11 child metadata, shapes, and reusable picture objects with correct
   invalidation on resize, movement, visibility, destruction, and stacking
   changes. The current map path re-queries them and rebuilds picture objects
   each frame. The prior wait-only cleanup does not remove that repeated work.
3. Reuse producer snapshot allocations only after consumers release ownership.
   Every composition Present currently allocates and copies a fresh texture,
   including the matched hidden target. Preserve the latest complete frame
   and immutable ownership. Do not overwrite a frame still held by a consumer
   or fake occlusion to reduce work.
4. Track changed regions so small browser updates do not force copying and
   blending entire images. Never publish a partially updated completed frame.
5. Longer term, keep browser images on the graphics device instead of copying
   them to CPU memory and uploading them to X11. The current staging copy,
   synchronous map, CPU copy, and image upload are visible in source. Replacing
   that path requires graphics sharing and synchronization work, not a
   registry setting. It is a larger change than allocation reuse or caching.

Each performance candidate needs the same manually verified scene, panel-open
timing, frame-time and CPU measurements, and checks for borders, white flashes,
flicker, hidden panels, and unrelated-window stacking. No speedup or regression
should be attributed to a source patch from a simultaneous font, power, or
graphics configuration change.

### October 15 manually accepted smooth checkpoint

The owner reports that the restarted game is now butter smooth and has the
highest performance seen in this investigation. This is the accepted combined
runtime checkpoint. The owner also reports that emoji and fonts still do not
match Windows 11. The supplied settings screenshot shows regular text and
several missing-character boxes alongside simpler symbols. Font availability
and a successful color-layer unit probe did not establish working browser
fallback for these characters.

Do not claim that the font corrections alone caused the performance change.
The accepted state includes the existing composition implementation, the
previous XRender wait cleanup, corrected font registrations, a restarted
browser using the existing profile, and the competing Windows VM shut down.
There was no controlled comparison isolating these factors. Earlier timings
did not show a speedup from the wait cleanup alone.

The exact Wine source used by the installed driver is committed as
`97dda5089c2e92d3236a50dc30257a772ec28d24` on `dcomp-composition`, following
the previously pushed accepted checkpoint `057a04bc`. Its only source change
from that checkpoint is the nine-line XRender wait cleanup already running
during the accepted test. This commit preserves the tested state and is not
evidence that the cleanup by itself fixed performance.

Both the built and installed native driver have SHA-256
`ad454dbdb8dfc67b56dcff5216fbdf0df33b4e47b1b5a54f70ff50978029214d`.
The installed composition DLL remains
`1de351f8fd34098b9d6e1dfa3fab887cc56d4accb2afe10810bba98b4499cdb5`.
The power profile remains Balanced. DreamSeeker retains its OpenGL backend and
WebView2 its Vulkan backend. The recorded accepted launch is
`20261014-193241-game`, reusing browser profile `20261014-185951-game`.

The project checkpoint includes `config/byond-fonts.reg`, its undo file, and
this diary. Runtime font files are intentionally not committed. To preserve
the exact local bytes, a 2.5 MiB private archive was created at
`test-results/safety/soa-smooth-fonts-20261015.tar.gz`. It contains the thirteen
added font files, the three existing core-font files whose regular registrations
were restored, the retained license notices, and both registry configurations.
Its SHA-256 is
`9b3bb96f0b7898626affb4b02ef5560d17bddb26083f3e156aaa71999354576a`.
The archive is ignored by Git, has been listed and checked, and does not contain
browser profiles, cookies, saved credentials, game worlds, or recordings.
It is a local recovery aid, not permission to redistribute the included fonts.

Appearance work remains separate. [Microsoft documents Segoe UI Emoji](https://learn.microsoft.com/en-us/typography/font-list/segoe-ui-emoji)
as the Windows emoji font. Twemoji is different artwork, and Selawik and Carlito
are substitute text fonts. Installing substitutes cannot establish an exact
Windows match. Matching glyph designs would require matching font files and
versions, correct fallback, and compatible text and color rendering. Pixel
identical output is not guaranteed. [Microsoft's font license FAQ](https://learn.microsoft.com/en-us/typography/fonts/font-faq)
also restricts redistribution, so Windows fonts must not be added to a public
Linux package without the relevant rights.

The next appearance test should identify the exact Unicode code points in the
settings buttons and their actual rendered fonts, then check coverage and font
selection for those characters. This distinguishes absent drawings from a
failure to select an installed font. Do not rewrite the game stylesheet, change
graphics settings, or alter composition while running that test. The test
session was no longer running at checkpoint creation and was not relaunched
or changed during this commit operation.

### Clean normal-play launch after the smooth checkpoint

The owner requested optimized play without diagnostic overhead. The accepted
Wine build already uses `CFLAGS = -g -O2`. No rebuild, stripping, graphics
change, power-profile change, or font change was performed. Debug symbols are
retained. DreamSeeker and WebView2 remain the prebuilt vendor executables, not
applications that can be switched to a different compiler configuration here.

`tools/play-soa.sh` is a dedicated normal-play entry point, not the test runner.
It uses the accepted installed runner, isolated prefix, graphics overrides,
and existing browser profile `20261014-185951-game`. It sets `WINEDEBUG=-all`
and `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--disable-logging`, clears inherited
browser script-debugging variables, and discards standard output and standard
error. It starts no capture, profiling, process-sampling, ad-skipping, or
debugging helpers and creates no test-results directory or diagnostic log.
The normal advertisement countdown remains because its test skip mechanism
required browser debugging. The launcher refuses to start a duplicate
DreamSeeker process.

The launch was verified once from the game environment and browser command
line. Both standard output descriptors point to the null device, the browser
has the logging-disable switch, and no listener exists on debug port 9242.
Both BYOND diagnostic file sizes and modification times were unchanged at the
23-second startup check. This is not a guarantee that BYOND, Chromium's normal
profile storage, the graphics driver, or the operating system never writes an
internal file. Their own required files and crash behavior have not been
replaced or disabled to claim a literal system-wide zero-log guarantee.

Run this mode again with `bash tools/play-soa.sh` from the project directory.
This is a logging-only runtime comparison against the smooth checkpoint.
No performance improvement is claimed before manual verification, and no
ongoing diagnostic process was left attached to the game.

### Docked chat failure during the clean play run

The owner reports that embedded panels still open very slowly and that docking
chat on the right leaves a solid orange area. This qualifies the earlier smooth
checkpoint: movement was manually accepted as smooth, not every panel-open or
docking path. Do not relabel the reported failures as working based on the
earlier acceptance.

Read-only inspection of live DreamSeeker process `1946871` confirms the same
isolated prefix, persistent browser profile, DLL overrides, and installed
binary checksums as checkpoint `97dda508`. The native driver remains
`ad454dbdb8dfc67b56dcff5216fbdf0df33b4e47b1b5a54f70ff50978029214d`, and the
composition DLL remains
`1de351f8fd34098b9d6e1dfa3fab887cc56d4accb2afe10810bba98b4499cdb5`.
No source or font rollback was found. The clean launch uses `WINEDEBUG=-all`
and `--disable-logging` without a browser debug port. Differences in timing
between launches still require measurement rather than attribution.

The native docked composition child is `0x3a00016`, sized 300 by 740 at
position 1066,0 inside the 1366 by 740 game client. A short in-memory debugger
inspection associates this geometry with composition target `0x107fe` and
surface `0x307be`. Its cached browser image, assembled frame, and saved
published frame all contain varied colors rather than a uniform orange fill.
A 153-point color sample found 36 distinct colors in each of these buffers.
This is evidence that the saved image is not simply blank orange. It is not
a visual verification of the complete current chat document.

At inspection time the game client was unmapped, so all target visibility
flags were zero. That is not evidence that the windows were hidden incorrectly
during the screenshot. The owner was asked to restore the game and report
whether the orange area remains. The browser probe detached normally. A
separate five-second bounded map-presentation probe captured no calls while
the game was unmapped and also detached. No chat text, screenshots, or video
were saved, no continuous logging was enabled, and the game was not restarted.

Source inspection identifies a clipping mistake worth testing.
`X11DRV_client_surface_present` manually redirects the parent window's alpha
children for composition, but passes the individual map client's drawing
region into `X11DRV_XRender_CompositeChildren`. That region is then intersected
with each independent child's shape. A sibling docked outside the map can
therefore be taken out of native presentation and also excluded from the
manual composition. The exact live map clipping region was not captured, so
this remains the leading source-level explanation, not a proven complete
diagnosis of the owner's screenshot.

One candidate changes only `dlls/winex11.drv/init.c`. It leaves the map drawing
region in force for map pixels, but does not pass that region as a restriction
on independent child surfaces. Each child's native shape remains in force,
and publication to the real parent window still has native window clipping.
The candidate is four added lines and one removed line relative to checkpoint
`97dda508`. It compiles and passes `git diff --check`.

A rectangle-only X11 region check used a map width of 1066 and a docked child
at 1066,0 with size 300 by 740. The old intersection is empty. A floating child
fully inside the map retains its bounds, while a child crossing the map edge
is incorrectly reduced from width 300 to 66 by the map restriction. Removing
that restriction retains each independent child's own bounds. These are
region-logic checks, not an SOA reproducer, visual acceptance test, or complete
Wine regression suite.

The candidate was built but not installed. The running game and installed
driver remain on the accepted checkpoint. The built driver now differs from
the installed one, so further live debugger work must load symbols from the
installed native driver, not assume the build output matches it. No commit or
performance improvement is claimed for this candidate. The slow panel-opening
problem remains separate and has not been explained by this clipping change.

### Authorized diagnostic restart for the docked chat baseline

The owner authorized restarting the same installed checkpoint with diagnostic
logs. The ordinary DreamSeeker close request did not exit the hidden session.
Only the isolated test prefix was then stopped with its runner's wineserver,
and its old game and browser processes were verified gone before relaunch.

The new DreamSeeker process is `1955287`, browser process `1955377`, and GPU
process `1955964`. The installed driver and composition DLL checksums still
match the accepted checkpoint. The candidate clipping change remains built
but uninstalled. Fonts, graphics backends, registry settings, and persistent
browser profile remain unchanged.

This baseline enables Wine errors and focused warnings for composition,
DXGI, D3D11, and X11 presentation. Timestamp and process prefixes are enabled.
It deliberately avoids continuous per-frame tracing. The output is drained
continuously and retained up to eight MiB in
`/tmp/soa-docked-chat-baseline-20261015.log`. Browser logging remains disabled.
No recording, screenshot capture, browser debugging port, or ad-skipping
helper is running. Initial inspection confirms the advertisement window is
open. The initial Wine log is empty, which is not evidence that composition
works. Manual login and a visible docked-chat failure are required before the
bounded clipping trace can establish the actual map drawing bounds.

### Visible docked-chat baseline measurements

The owner confirmed the orange docked-chat area was visible. The main native
game window was verified viewable during the following measurements. The
installed binaries were not changed and no debugger was left attached.

Expected behavior is readable chat in the 300-pixel-wide right dock. Observed
behavior is the owner's orange strip, with the map occupying the left 1066
pixels of the 1366-pixel-wide client.

Three native frame samples measured the actual XRender clipping region as
one rectangle from 0,0 to 1066,740. Independently composed browser children
included a 17 by 17 child at 1345,479 and a 102 by 44 child at 1253,496.
Both lie outside the region supplied by the map presenter. This confirms the
source-level clipping mistake affects real browser siblings outside the map.
It does not by itself establish the cause of the orange chat area.

The dock-sized native surface `0x3a00016` was unmapped while the game itself
was viewable. Its composition target `0x10804` had a saved published frame
and a 300 by 740 surface, but `surface_visible` and position validity were
zero. A separate read-only Win32 probe found that its parent chain first
loses `WS_VISIBLE` at BYOND dialog `0x1077e`, class `#32770`, style
`0x44000044`. Its Chrome descendants retain their own visible style bit,
but `IsWindowVisible` correctly reports them hidden because of that ancestor.
The synthetic surface `0x507c8` is also hidden. The probe read only class names,
geometry, styles, and visibility. It did not read chat or login text.

Only this hidden browser chain and its hidden synthetic surface matched the
dock dimensions during enumeration. Other small browser targets had visible
chains and active synthetic surfaces. Therefore installing the clipping-only
candidate now would not establish a complete chat fix. In particular, forcing
all hidden browser surfaces visible would also revive intentionally closed
panels and the advertisement. That is not an acceptable correction.

The current model has two distinct issues: the map clip excludes independent
visible siblings, and the dock-sized browser is currently under a hidden
BYOND control. It remains unknown whether this control is the current chat
host or an old hidden host retained after docking. The next discriminating
test is one manual undock and redock cycle, comparing the active browser's
parent, visible style, and geometry before and afterward. No source or
configuration change is needed for that comparison.

The warning log contains repeated unsupported-interface queries and device
status stubs, plus startup service and authentication warnings. These messages
have not been correlated with the missing chat or panel-opening delay and
must not be called their cause. The output remains capped at eight MiB.

### Dock controls are inaccessible in the failed state

The owner clarified that the undock button is part of the missing chat page.
The dock shortcut also does not recover the page. The proposed manual undock
and redock test therefore cannot be performed through those controls. Do not
repeat that request or describe the test as completed.

The game remains open on the unchanged installed checkpoint. A brief main
thread stack sample did not identify a persistent input deadlock. One sample
without complete symbols cannot rule out stalls or explain the failed shortcut.
No window was forced visible and no application control was changed.

Read-only inspection of the cached interface source found the actual chat
button actions in `cache/tmp32/7666688c.htm`. The `btnFloat` function calls
`BYOND.command('VMChatSetMode float')`. The right-dock button calls
`BYOND.command('VMChatSetMode dock_right')`. The page also saves settings,
and its `setDocked` function receives a state update from the game. This is
evidence of the intended actions, not evidence that the current live browser
will execute them successfully. No cached interface source was edited or
added to the repository, and no chat messages or login inputs were inspected.

The next test can invoke the same ordinary interface action through browser
inspection, without a missing button or shortcut. It should then compare the
parent visibility and geometry against the current failure. If the action
executes but no state changes, the investigation must include the game command
handoff rather than assume the only problem is final pixel presentation.
The current session has no browser debugging connection. Enabling one requires
an authorized restart and another manual login. Keep the clipping candidate
uninstalled for that baseline comparison.

### Browser inspection enabled for the dock recovery test

The owner authorized another restart with browser inspection enabled. The
normal close request did not end the previous session, so only the isolated
test prefix was stopped before relaunch. The new DreamSeeker process is
`1993498`, browser process `1993584`, and GPU process `1993745`.

The installed native driver and composition DLL hashes still match checkpoint
`97dda508`. The clipping candidate remains uninstalled. The persistent browser
profile, font configuration, registry, graphics backends, and focused Wine
warning settings are unchanged. Browser arguments add only local inspection
at port 9242, with logging still disabled. The listener was verified bound to
`127.0.0.1`, not to all network interfaces.

Wine output is retained up to eight MiB in
`/tmp/soa-docked-chat-inspection-20261015.log`. No recording, screenshot capture,
automatic ad action, or interface command has been run. The advertisement
window is open. The next action requires the owner's manual login. Inspect
only the chat document's interface state, not login fields or chat messages,
then attempt its ordinary float action and compare window visibility.

### Live chat undock action and stale composition bounds

The owner confirmed the orange strip remained after logging in. Browser
inspection identified the live chat document by `chat-panel` and its existing
`btnFloat` function. Its initial dock mode was `right`, viewport 300 by 740,
and document visibility was `visible`. No input values or chat text were read.

Unlike the previous baseline run, this run's dock-sized browser chain was
visible before any action. The BYOND dialog was `0x10786`, style `0x54000044`,
its DComp target was `0x10818`, and synthetic surface `0x10860` was visible at
1066,0 with size 300 by 740. Thus the earlier hidden-control observation is
not a complete explanation for the persistent orange strip. The independently
confirmed map-clipping mistake remains relevant to this visible dock case.

Calling the page's existing `btnFloat()` function completed without a script
exception. Subsequent inspection found the dock attribute removed and the
browser viewport changed to 300 by 320. Native browser windows moved to
screen rectangle 8,440 to 308,760 and remained visible. This establishes that
the ordinary interface command can reach the game and update its layout,
despite the inaccessible button and failed shortcut. It does not establish
that the resulting chat image is readable. The owner was asked to verify
that separately, and no automatic redock action was issued while awaiting
that verification.

A bounded geometry-only debugger sample then matched target `0x10818` to a
cached browser image of 300 by 320, but its synthetic surface and published
bitmap remained 1358 by 1152 at client origin 8,0. Native geometry independently
confirmed that oversized surface. This is approximately 16.3 times the current
browser image's pixel area. It does not imply a measured 16-fold performance
loss or explain the entire panel-opening delay.

Source inspection links the retained size to `target_composite_surface`, which
unions the current image bounds with the existing surface bounds. Those old
bounds persist across frames, even after a panel moves or shrinks. The frame
is cleared, but its bounds are not recomputed solely from current visual
content. The next size correction must compute the union of current-frame
visuals while preserving all visuals within that frame. It must also handle
size-only changes in the geometry cache. Blindly shrinking to the first visual
or retaining the old cache shortcut would be incomplete.

Neither this bounds issue nor the clipping candidate has been installed or
committed. The current session still uses the accepted Wine binaries. The
debugger detached, no screenshot or recording was made, and the chat remains
in the floating mode requested through its normal interface action.

### Manual confirmation isolates the docked-layout failure

The owner confirmed that invoking the normal float action restored readable
chat and removed the orange strip. The owner then used the game's right-dock
action and reproduced the orange strip. A read-only browser query afterward
confirmed dock mode `right`, viewport 300 by 740, and document visibility
`visible`. This is a manually verified floating-versus-docked control result,
not an inference from successful frame publication.

The result supports the map-boundary clipping explanation. The chat document
loads and works while floating over the map, but disappears when positioned
outside that map's drawing region. It does not establish that every earlier
hidden-control state had the same cause, nor that this clipping correction
will resolve panel-opening latency.

The next single source test is the existing `init.c` correction: continue to
clip map pixels to the map region, but do not use that region to exclude
independent browser siblings. Their own native shapes and final window
clipping remain in effect. No changes to frame sizing, caching, visibility,
fonts, browser options, or graphics backends are included in this candidate.

The native driver target builds successfully, and `git diff --check` passes.
The accepted installed driver was copied without overwriting any existing
backup to `test-results/safety/winex11-before-docked-chat-20261015.so`. Its
SHA-256 matches the installed checkpoint:
`ad454dbdb8dfc67b56dcff5216fbdf0df33b4e47b1b5a54f70ff50978029214d`.
The backup is ignored by Git. The candidate remains uninstalled and the game
remains open. Restart permission is needed before replacing the driver and
repeating the owner's docked-chat verification. The oversized-frame correction
is explicitly deferred so its result is not mixed with the clipping test.

### Clipping-only candidate installed for manual verification

The owner authorized installation and clarified that routine SOA test restarts
should proceed without repeated permission questions. Keep the owner informed,
preserve recoverable checkpoints, and ask for manual verification afterward.
This does not authorize unrelated configuration changes or operating-system
actions.

After a normal close request, the isolated test prefix was stopped and its old
game and browser processes were verified gone. Only the native X11 driver was
replaced with the built clipping candidate. Its installed SHA-256 is
`e249b6ce8c541c6d23e55d1d96e741470a0c6796b11b1fe3cd5348f5ebd10010`.
The DirectComposition DLL remains unchanged at
`1de351f8fd34098b9d6e1dfa3fab887cc56d4accb2afe10810bba98b4499cdb5`.
The prior native driver remains available in the recorded safety backup.

SOA was relaunched as DreamSeeker process `2856893`, browser `2857044`, and
GPU process `2857203`. The game has loaded the installed native driver. The
advertisement window is open and browser inspection still listens only on
local port 9242. The same prefix, persistent browser profile, Wine diagnostic
settings, browser arguments, fonts, and graphics backends were retained.
The new bounded Wine log is `/tmp/soa-docked-chat-clipping-20261015.log`.
No capture, ad action, commit, or second source change was performed.

Expected behavior is readable right-docked chat without an orange strip while
floating chat and map rendering continue to work. The owner must log in and
verify this visually. Startup and successful driver loading are not acceptance
of the fix. The oversized-frame issue and slow panel-opening work remain
deferred and are not claimed fixed by this test.

### Accepted docked-chat checkpoint

The owner manually confirmed that right-docked chat is now readable and
requested that this state be committed and pushed as the best checkpoint so
far. The accepted source change is Wine commit
`878fea5fd6c32c517024db1efc34fceeb2a97855`, titled
`winex11: Keep redirected siblings outside the client viewport visible.`
It changes only `dlls/winex11.drv/init.c`, with four added lines and one removed
line. The previously recorded installed native driver hash identifies this
accepted runtime. The DirectComposition DLL is unchanged.

The checkpoint includes this investigation record and `tools/play-soa.sh` in
the project repository. The latter is the existing normal-play launcher,
validated with `bash -n`, not a new test harness. Runtime profiles, fonts,
credentials, screenshots, logs, and binary safety backups are not included.

The owner also reported that tooltips over embedded browser panels appear as
white boxes. The supplied screenshot shows readable right-docked chat controls
and a small white rectangle below a hovered control. This is an outstanding
defect, not a reason to mark the accepted clipping correction unsuccessful.
Its rendering path has not yet been identified. In particular, do not assume
it is a page element rather than a separate browser tooltip window.

The next tooltip diagnostic should distinguish an in-page element from a
native popup by comparing browser interface metadata and native window class,
geometry, and visibility during hover. No chat text or login fields are needed.
Keep this accepted build running and do not combine a tooltip change with the
deferred oversized-frame correction or other performance work.

### White tooltip diagnostic and first candidate

Expected behavior is a readable tooltip over the existing browser control.
The owner reports a plain white box over every embedded browser panel. The
accepted docked-chat commit remains the recovery checkpoint for this work.

The chat toolbar uses ordinary HTML `title` attributes, including Smaller font
and Larger font. A real native hover reproduced a separate
`Chrome_WidgetWin_1` popup, initially 81 by 25 at screen position 1274,53.
Its style was `0x96000000` and extended style `0x08200028`. It is not a DOM
tooltip element inside the chat page. Browser-generated hover events alone
were insufficient to obtain the native popup reliably. A window-manager
activation request and X11 pointer motion with a synchronous request check
provided a repeatable hover without clicking any game control.

[Chromium 109's TooltipAura source](https://chromium.googlesource.com/chromium/src/+/refs/tags/109.0.5414.119/ui/views/corewm/tooltip_aura.cc)
requests software compositing for its tooltip widget. This is source context,
not proof that the Edge fork follows a purely GDI path in this session. The
live popup also contains a GPU-owned `Intermediate D3D Window`. Its visible
Win32 style is `0x5c000000` and extended style `0x00280024`, which combines
layering with the no-redirection-bitmap flag. Its native window was created
under a hidden helper and was unmapped in the sampled window tree.
[Chromium's child-window implementation](https://chromium.googlesource.com/chromium/src/+/refs/tags/109.0.5414.119/ui/gl/child_window_win.cc)
is relevant to that separate graphics helper, rather than an HTML layout fix.

Bounded debugger probes covered selected GDI uploads, normal DXGI presentation,
composition target creation, and composition surface stubs. Most captured no
matching tooltip call. Those negative samples do not prove that each API is
unused, particularly with transient popups and optimized functions. The
browser-owner probe captured one small surface flush with an 88 by 25 dirty
area in a 128 by 128 backing allocation and only one RGB value. It did not
capture a completed tooltip-text upload. An X11 creation breakpoint identified
the GPU's small window through its Window owner thread. X11 resource queries
independently matched the helper connection to the GPU process. All debugger
probes detached normally. No screenshot, video, chat text, or login input was
captured.

Source review found an overbroad rule added by our earlier layered-child
support. Both native child-window creation and preservation during reparenting
treat every `WS_EX_LAYERED` child as a bitmap-backed surface, including the
graphics helpers with `WS_EX_NOREDIRECTIONBITMAP`.
[Microsoft documents the latter flag](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
as opting out of a redirected surface, including windows whose visuals use
another mechanism. The current hypothesis is that extending the bitmap-backed
native-window rule to these graphics-only children interferes with their
ordinary presentation. This has not yet been proven to be the complete cause
of the white tooltip.

The first candidate changes only `dlls/winex11.drv/window.c`. The extra native
window and preservation rules now require `WS_EX_LAYERED` without
`WS_EX_NOREDIRECTIONBITMAP`. Top-level window handling is unchanged. Ordinary
graphics-only children return to the pre-existing child path, while our
synthetic `Static` composition surfaces retain independent native windows.
The accepted sibling-clipping correction is untouched. No class-name check,
HTML edit, font change, graphics-backend change, frame-size optimization, or
visibility-forcing workaround is included.

The candidate builds with the existing optimized flags and passes
`git diff --check`. A five-case flag-selection check covers ordinary children,
bitmap-backed layered children, no-bitmap graphics helpers, the actual tooltip
extended style, and the actual accepted synthetic-surface style. This is not
a Windows conformance test or visual proof.

The accepted native driver is backed up at
`test-results/safety/winex11-docked-chat-checkpoint-20261015.so`, with SHA-256
`e249b6ce8c541c6d23e55d1d96e741470a0c6796b11b1fe3cd5348f5ebd10010`.
Only the rebuilt native driver was installed for the candidate. Its SHA-256 is
`91e59c49dfb735d79876e78c71f584aed8858395003b4f221fe8d0367246c8bb`.
The DirectComposition DLL remains unchanged. The source change is uncommitted.

The isolated session was restarted as DreamSeeker `3051513`, browser `3051663`,
and GPU `3051826`, retaining the same prefix, persistent profile, browser
arguments, diagnostic settings, fonts, and graphics backends. The advertisement
window and local inspection listener were verified open. The bounded log is
`/tmp/soa-tooltip-layered-child-20261015.log`. The owner must now verify readable
tooltips and continued readable right-docked chat. Do not call this candidate
accepted, fixed, or faster based on startup alone.

### Tooltip candidate rejected and accepted docked-chat build restored

The owner reported that the orange docked-chat area returned during the
tooltip candidate. This is a regression, not a successful tooltip result.
The flag-selection check and successful compilation did not validate visible
composition. No tooltip improvement was accepted.

Both changes in `dlls/winex11.drv/window.c` were reverted. Wine source is clean
at accepted commit `878fea5fd6c32c517024db1efc34fceeb2a97855`. The isolated
test session was stopped before replacing its driver. The saved accepted
native driver was restored, and the reverted source was rebuilt separately.
The installed driver and rebuilt driver both have SHA-256
`e249b6ce8c541c6d23e55d1d96e741470a0c6796b11b1fe3cd5348f5ebd10010`.
The DirectComposition DLL remains unchanged, with SHA-256
`1de351f8fd34098b9d6e1dfa3fab887cc56d4accb2afe10810bba98b4499cdb5`.

SOA was relaunched with the same test prefix, persistent browser profile,
graphics backends, fonts, browser arguments, and bounded diagnostic settings.
The recovery log is `/tmp/soa-tooltip-recovery-20261015.log`. No candidate
source was committed or pushed. No new tooltip fix or performance change was
combined with recovery. Manual confirmation of readable docked chat is still
required after login.

The recovery DreamSeeker process is `3325899`. Its loaded native driver path
was verified and no debugger is attached. The inspection listener remains
restricted to `127.0.0.1`. Sixteen orphaned browser processes from the rejected
session survived the isolated Wine server shutdown. Their process identifiers,
browser process names, and exact test-prefix environment were checked before
sending normal termination signals. The new session was not targeted.

The hidden graphics-helper observation remains evidence, but the proposed
no-redirection-bitmap exclusion is rejected as a usable correction. Its role
in the white tooltip remains unproven. The next tooltip investigation must
explain why the accepted docked browser also needs the behavior removed by
this candidate before proposing another change. Do not force hidden browser
windows visible or repeat the rejected exclusion as if it were validated.

### Accepted recovery checkpoint and remaining defects

The owner manually confirmed that right-docked chat is readable again after
recovery and requested that this state be committed and pushed as the highest
accepted checkpoint. Wine remains at `878fea5fd6c32c517024db1efc34fceeb2a97855`.
There is no new Wine source change to commit. The installed driver and
DirectComposition hashes recorded above identify the restored runtime.
The rejected tooltip candidate is not part of this checkpoint.

Three defects remain open, and acceptance of docked chat does not imply that
any of these are fixed:

1. Browser tooltips appear as white boxes instead of readable labels.
2. Inventory, skill tree, and other embedded HTML panels take far too long
   to open. The current report is qualitative, not a measured timing result.
3. The owner reports incorrect rendering of particle effects. The supplied
   screenshot shows a large white glow where the owner expects red fire
   particles. It demonstrates the visible mismatch, but cannot establish
   whether all particles are affected or identify the responsible graphics
   operation. This is tracked separately from browser composition.

Continue one experiment at a time, starting with the previously observed
oversized composition frame. Expected behavior is prompt panel opening and
a backing frame matching the current visual bounds. Observed behavior is
slow panel opening and an earlier measured backing frame about 16.3 times
the floating chat area. The current model is that stale frame bounds cause
unnecessary clearing, copying, and blending. That may contribute to latency,
but has not been proven to explain the full delay. The next test must measure
frame bounds before and after a resize while preserving all active visuals,
then ask the owner to verify panel responsiveness and docked-chat rendering.
Do not combine this change with tooltip handling, particle blending, fonts,
or a graphics-backend switch.

### First panel-latency candidate after the accepted recovery

The confirmed recovery record was committed and pushed as project commit
`c704495efdf1d7fc206de6c5674adb6825688e1b`. The personal Wine branch is also
pushed at accepted commit `878fea5fd6c32c517024db1efc34fceeb2a97855`.
That is the recovery point, not the following unaccepted candidate.

The single candidate changes three DirectComposition files:
`dcomp_private.h`, `device.c`, and `target.c`. It does not modify the accepted
native X11 driver, browser arguments, fonts, registry, particle rendering,
or tooltip handling.

Each composition frame now records the union of the visuals actually blended
in that frame. At publication, the working bitmap is cropped to that union
once, after all visuals have contributed. Bitmap resizing copies only the
intersection between old and new bounds. This handles shrinking and negative
coordinates without assuming that the new allocation contains the old one.
It preserves multiple visuals and their existing premultiplied alpha blending.
The unchanged-frame shortcut also checks both destination and target client
rectangles, rather than treating unchanged position as unchanged geometry.

The focused temporary test at `/tmp/dcomp-frame-bounds-20261015.c` includes
the actual `target.c`, rather than a separate model of its arithmetic. Against
the accepted implementation, 10 of its 28 checks fail because old bounds and
native dimensions persist. Against the candidate, all 28 pass. Cases include
large-to-small frames, negative origins, separated visuals, transparent gaps,
reversed traversal order, movement, overlapping translucent visuals, removal
of a visual, and native surface size after shrinkage. This is an internal
regression check, not a Windows conformance test or proof that SOA opens panels
faster. The candidate also builds with the existing optimized flags and passes
`git diff --check`.

Wine's existing `dlls/dcomp/tests/dcomp.c` was built temporarily as a standalone
test because this configured build has no generated DirectComposition test
target. An initial run accidentally used the prefix's DXVK defaults and was
discarded. A baseline run with the game's built-in graphics overrides failed
all three screen-color checks and did not complete. Its test process was
terminated before restarting the isolated prefix. The failures are already
present on the accepted baseline and cannot be used as evidence for this
candidate. They also mean that no passing full-suite result is claimed.
The baseline output is `/tmp/dcomp-suite-baseline-20261015.log`.

Before installing the candidate, the accepted DirectComposition DLL was saved
as `test-results/safety/dcomp-docked-chat-checkpoint-20261015.dll`, with SHA-256
`1de351f8fd34098b9d6e1dfa3fab887cc56d4accb2afe10810bba98b4499cdb5`.
The installed candidate DLL has SHA-256
`df3932b6cc9a6c6237c955b8b25a3d369ecf660c845a4c24edfa11d456b41031`.
The native X11 driver remains the accepted
`e249b6ce8c541c6d23e55d1d96e741470a0c6796b11b1fe3cd5348f5ebd10010`.
Candidate source is deliberately uncommitted pending manual acceptance.

A preliminary launch with fewer warning channels was stopped before manual
verification so that logging would not become a second experimental variable.
The final run uses exactly the recovery run's diagnostic flags, browser
arguments, profile, graphics backends, and prefix. DreamSeeker is `3339830`,
the GPU process is `3340091`, and the local inspection listener is restricted
to `127.0.0.1:9242`. The bounded final log is
`/tmp/soa-frame-bounds-game-20261015.log`.

The owner was asked to confirm readable right-docked chat and time both the
first and second opening of inventory and skill tree. No timing improvement,
tooltip fix, particle fix, or final acceptance is claimed before that reply.
If chat regresses, restore the backed-up DLL before continuing. If timings
remain poor, the next latency measurement must separate browser page readiness
from first visible frame publication. A smaller bitmap alone cannot establish
whether the remaining wait occurs before or after the browser draws.

### Latency candidate deferred and investigation order changed

The owner reported no noticeable improvement from the buffer-sizing candidate.
HTML panels still take a long time to open. No numerical timings were supplied,
so this is a negative manual responsiveness result, not a measured speedup.
Passing the internal frame-bounds test does not override that result.

The owner explicitly set the remaining order to particles, tooltips, fonts,
then panel-opening latency, one issue at a time. Font symptoms include possible
wrong face, wrong color, and missing black outlines. Do not assume all font
symptoms are font-file problems. Do not investigate these deferred issues in
parallel with the current particle test.

The unaccepted latency patch was preserved without a commit at
`test-results/safety/dcomp-frame-sizing-20261015.patch`, with SHA-256
`5fdad8145756e7574bed4bc3ef5ab571cbbeb78e9620414452d83c6bf039a7a8`.
Its three source changes were then reverted. Wine source is clean at accepted
commit `878fea5fd6c32c517024db1efc34fceeb2a97855`. The accepted composition DLL
will be restored for the particle experiment. The retained patch is deferred,
not accepted and not an established fix for panel-opening latency.

### Particle experiment one: select the installed Microsoft D3DX9 helper

Expected behavior, reported by the owner, is colored fire particles. Observed
behavior is a large white glow in the supplied SOA screenshot. The screenshot
alone cannot identify the shader or prove which graphics operation is wrong.

The live DreamSeeker process `3339830` maps the installed Wine implementations
of `d3d9.dll`, `wined3d.dll`, `d3dx9_43.dll`, and `d3dcompiler_43.dll`.
Its application-specific renderer remains OpenGL. The native helper DLLs are
already present through existing system-directory links to the Lutris runtime,
but their presence does not mean the process selected them.

Static import inspection of BYOND's `byondwin.dll` confirms calls to
`D3DXCreateEffect`, `D3DXCreateSprite`, and matrix helpers through `d3dx9_43`.
The same engine binary contains readable shader source for color matrices,
outlines, blur, and other effects. This establishes relevant use of D3DX9,
not the exact rendering path of the failed SOA fire effect. No SOA server
source was accessed or changed.

The existing native D3DX9 helper has SHA-256
`0b28546be22c71834501f7d7185ede5d79742457331c7ee09efc14490dd64f5f`.
Streaming the matching DLL from BYOND's bundled
`Jun2010_d3dx9_43_x86.cab` produces the same hash. No download, installation,
registry change, or replacement of a shared Lutris runtime is needed.

The current hypothesis is a difference in effect handling between Wine's
helper and Microsoft's helper. It is not proven to explain the particle
failure. The discriminating test changes only helper selection by adding
`d3dx9_43=n,b` to the existing launch overrides. Keep `d3d9`, `d3d11`, and
`dxgi` built-in and retain the existing OpenGL game renderer, Vulkan browser
renderer, browser profile, and accepted composition binaries. Do not also
force the Microsoft shader compiler in this first test. Inspect the process
maps after launch to verify the actual selected libraries, then ask the owner
to repeat the same particle effect. This test intentionally does not change
tooltip handling, fonts, or panel-latency code.

The accepted composition DLL and native X11 driver were restored and verified
with their previously recorded hashes. The rebuilt source tree is clean.
The particle-only run is DreamSeeker `3365508`, with bounded log
`/tmp/soa-particles-native-d3dx9-20261015.log`. Its process maps confirm the
Microsoft D3DX9 helper is selected from the existing Lutris runtime, rather
than the installed Wine helper. The local browser inspection listener is open
on `127.0.0.1:9242`. Actual particle colors still require manual verification
after the game loads. No result is inferred from DLL selection alone.

### Owner distinguishes visible particles from incorrect emitted light

The owner supplied a closer screenshot and clarified that particles are
visible, but also emit light. The crop shows colored fire detail together
with a much larger pale glow. This disproves the broad description that no
particles render. It does not by itself prove that every particle property
is correct or identify which draw operation produces the incorrect glow.
The leading investigation is now the light or glow contribution around the
effect, not replacing the particle system.

Expected behavior is the fire and its surrounding illumination matching the
Windows game. Observed behavior is visible colored detail with the reported
incorrect pale illumination. The current model is a possible error in the
light image, its intensity or color, or its blending into the scene. These
are hypotheses, not a proven root cause. A bright glow alone cannot tell
whether the light image was already wrong before blending or becomes wrong
when combined with the scene.

The current native-helper run still maps Microsoft's `d3dx9_43.dll` and Wine's
`d3d9.dll` and `wined3d.dll`. At the later sample there was no separate
`d3dcompiler_43.dll` mapping, so do not claim that forcing the native helper
left the actual compiler execution path unchanged. The original test did
not explicitly override the compiler, but its internals can differ.
No particle-specific Wine source was changed. Selecting the native helper
remains an unaccepted configuration experiment, not a demonstrated fix.
The owner's clarification is not a controlled comparison establishing that
the helper did or did not change the colored detail.

Read-only inspection found built-in BYOND shaders for color adjustment,
blur, thresholding, and other effects. Their presence does not identify
SOA's lighting implementation, and neither the general graphics warnings
nor the retained browser warnings prove a lighting failure. Do not mistake
WebView2 Vulkan fixed-function warnings for the game's OpenGL light pass.

The next useful diagnostic is to isolate the light contribution for the same
effect while leaving its particles unchanged, then inspect the input image
and blend state of the responsible draw. If SOA exposes a lighting control,
use that existing control for a comparison. Otherwise a targeted graphics
capture must identify the light draw before any Wine blend change is made.
Do not invent a light toggle or assume that a generic shader is the one SOA
uses. No game restart, new graphics setting, or additional Wine patch was
applied while recording this clarification. Tooltips, fonts, and panel
latency remain deferred in the owner's requested order.

### 2026-10-15: publish the accepted Wine checkpoint under the new account

The owner requested publication under `octaviothelatino`, without deleting
the former repository or moving this private helper project. The destination
is [the public Wine repository](https://github.com/octaviothelatino/wine).
It is an independent repository, not a GitHub fork. The official Wine remote
remains unchanged, so fetching official development remains possible.

Only the author and committer identity of our five custom commits changed.
The accepted checkpoint is now `6f9a860511ccb7981777da050f2a93c3b53227bd`,
on `dcomp-composition`. Its complete file tree is identical to the previous
accepted checkpoint `878fea5fd6c32c517024db1efc34fceeb2a97855`.
Official Wine history and author credits are unchanged. The installed
composition DLL and native X11 driver retain their accepted hashes.
This publication does not change the game or establish another visual fix.

The published branches are `master`, `dcomp-composition`, and
`d3d11-shared-resources`. The default is `dcomp-composition`. The base branch
is the existing official checkpoint, not a claim that it matches today's
newest official Wine commit. The five custom commits can be reviewed in
[the comparison view](https://github.com/octaviothelatino/wine/compare/master...dcomp-composition).

The full publication history was scanned for the retired identifiers agreed
with the owner. The scan covered 184,717 commits and 1,346,167 Git objects,
including historical file contents, and found no matches. Raw ASCII and both
UTF-16 byte orders were checked. Compressed fixture contents were not decoded.
None of our five commits adds such a fixture. First names and ordinary local
paths were deliberately excluded from the identity-removal scope.

The private upload was checked before publication. All five custom commits
are attributed to the new account on GitHub. Remote branch heads and the
accepted source tree were verified. Public access was then checked without
authentication. The former repository's branch heads remain unchanged.

The local `personal` remote now fetches and pushes to the new account. The
Wine repository's local commit identity uses the new account's verified
GitHub no-reply address. Global Git identity was not changed. A local pre-push
hook rejects outgoing objects containing the known retired identifiers.
A safe update passed its test and the original unsanitized checkpoint was
correctly rejected. These tests sent no push.

Original checkpoint references remain under `refs/private/pre-publication`
for local recovery. Do not push those references, mirror this checkout, or
upload the private commit mapping. Detailed audit and completion reports stay
in the ignored `test-results/safety` directory. No helper logs, game profiles,
screenshots, fonts, or copied game pages were published.

Removing known identifiers does not guarantee anonymity. The former public
repository, identical code, and external caches can still connect histories.
No deletion, transfer, or alteration of that repository was performed.
Lighting remains the next game investigation, followed by tooltips, fonts,
and panel-opening latency, in the owner's requested order.

### 2026-10-16: remove the push hook and update the official base

The owner clarified that the retired fake no-reply address was not sensitive.
The privacy scope is the real email addresses and retired GitHub username.
The local pre-push hook was unnecessary and prevented a normal master push
because it required historical file objects while disabling their on-demand
download. The repository's filtered fetch configuration can legitimately
leave such objects absent until needed. Do not reinstall this hook.

The exact installed hook was moved to the inactive local backup
`.git/hooks/pre-push.disabled-20261016`. There is no active pre-push hook.
The Wine source and its configured new-account commit identity were not
changed by disabling it. Private recovery references for both local branches
were saved under `refs/private/before-upstream-update-20261016`.

A fast-forward-only pull from official Wine confirmed local master at
`7b573fd7522d7e78957bdba1b175c717303923e8`. A normal push to `personal`
succeeded, updating the new GitHub repository's master to the same commit.
No bypass flag was needed.

A local trial merge of master into `dcomp-composition` found content
conflicts in `dlls/d3d11/texture.c`, `dlls/win32u/d3dkmt.c`,
`dlls/wined3d/context_vk.c`, `dlls/wined3d/texture_vk.c`, and
`dlls/wined3d/wined3d_vk.h`. The trial merge was aborted. The working tree
is clean on `dcomp-composition`, still at accepted checkpoint
`6f9a860511ccb7981777da050f2a93c3b53227bd`. No merge commit, source fix,
build, or game test was made. The updated official base has not yet been
integrated into the composition branch. Resolve and test these conflicts
before publishing that integration. The accepted runtime is unchanged.

### 2026-10-16: resolve the official-base merge and verify without installing

The owner authorized resolving the conflicts. The merge from master was
reopened on `dcomp-composition`, stopping before a commit. All five conflicting
files are resolved and staged. `git ls-files -u` is empty, the working files
match the staged resolution, and `git diff --cached --check` passes.
HEAD remains the accepted composition checkpoint, with official master
`7b573fd7522d7e78957bdba1b175c717303923e8` recorded as the pending merge
parent. No merge commit or push was made.

The resolution keeps official texture-usage changes while retaining our
shared-texture creation flag. Vulkan image creation now accepts the official
`VkImageCreateInfo` description structure for ordinary, exported, and imported
images. The shared path copies the description and preserves its existing
extension chain when adding external-memory information. Both the declarations
and callers were adapted. Shared-resource metadata retrieval now uses the
official resource allocation and cleanup helpers, preserving our global
resource update and query operations.

Compilation found an additional compatibility issue in an automatically
merged file: `dlls/d3d11/device.c` still called `wined3d_usage_from_d3d11`
with one argument. It now supplies the texture's miscellaneous flags as the
new second argument. Diagnostic format warnings in that file and
`dlls/win32u/window.c` were corrected without changing rendering behavior.

The existing shared-resource test also required adaptation. Its legacy case
combined two flags that official Wine now explicitly rejects. It now uses
the valid keyed-mutex sharing flag alone. The successful legacy operations
are required successes rather than stale expected-failure assertions. The
NT-handle path retains its expected-failure markers because that tested path
still returns `E_NOTIMPL`. Assertions were not removed or converted to skips.

The build initially lacked the test rules because the checked-in generated
configure script does not include our added test directory from configure.ac.
A generated `configure-merged` was placed in the existing external build
directory and used to regenerate its rules. The missing temporary Vulkan
linker alias was restored to the installed 32-bit loader. Vulkan support
and the DirectComposition test target are present in the regenerated build.
No tracked generated configure file was changed. The generated Autoconf
cache was moved from the source checkout into the existing build directory.

The affected graphics modules and both test binaries built successfully,
together with the loader, server, core runtime, Vulkan module, and Wineboot
needed to execute them. The final build log is
`test-results/safety/wine-merge-build-20261016-retry.log`. The subsequent
test-only rebuild also succeeded. Neither build ran an install target.

Expected verification behavior is successful resource handoff and correct
DirectComposition test colors without X11 errors. The observed controlled
results used Vulkan and temporary test prefixes, not either game prefix:

- The corrected shared-resource test passes against both the accepted
  installation and the merged build. In the merged run, the child reports
  three checks with no failures. The parent reports eleven checks, no
  unexpected failures, and one expected NT-handle failure. No checks skip.
- DirectComposition reports the same three black-pixel failures at test
  lines 698, 707, and 712 against both versions. The accepted installation's
  first run completes 116 checks with those three failures. Its repeated
  run stalls after reporting the same failures and is stopped at 90 seconds.
- Both merged-build runs report those three failures and then terminate
  with X11 `BadWindow` during `X_DestroyWindow`. This additional symptom
  is not explained or dismissed as a harmless baseline result.

The records are `wine-merge-repeat-tests-20261016.json` and
`wine-merge-baseline-repeat-tests-20261016.json` in the ignored safety
directory, with the corresponding test logs beside them. Test processes
were stopped using their own prefix-specific server, not the game server.

The current conclusion is that the source conflicts and compilation issues
are resolved, but runtime verification is not clean. Baseline color failures
cannot establish that the merge caused them, and successful resource-handoff
checks do not establish correct SOA rendering. The additional merged-build
window-destruction error still needs isolation before accepting the merge.
The next discriminating test should identify which native X11 window is
destroyed twice or after its destruction in this test, and compare the same
window lifetime in the accepted runtime. This is a hypothesis about the
error, not a proven cause. Do not remove assertions or modify unrelated
rendering code merely to make the test report pass.

The accepted installed composition DLL and native X11 driver still have
their recorded hashes. No SOA launch or visual acceptance occurred during
this merge task. The resolved merge remains staged and uncommitted for
review, and no new checkpoint is claimed.

### 2026-10-16: repository access, conflict reduction, and child-window cleanup

The owner asked whether public visibility lets strangers push branches.
Read-only GitHub inspection lists only `octaviothelatino` as a collaborator,
with the owner role, and no deploy keys. Public visibility does not itself
grant write access. No repository access setting was changed.

The owner also asked how to reduce merge conflicts. The update spanned
1,537 official commits, and three of the five conflicting files were one
coordinated Vulkan image-creation interface change. The current development
patch also spans shared graphics resources, composition, and X11 integration.
The proposed cleanup is smaller functional commits with focused tests,
minimal edits to existing entry points, removal of incidental diagnostics,
and adoption of new official helpers instead of duplicating them. New helper
files are appropriate for cohesive new responsibilities, not merely to move
modified official code away from its maintainers. Splitting commits improves
review and conflict isolation, but does not prevent textual conflicts by
itself. No branch rewriting or broad code reorganization was performed while
investigating the regression.

The owner authorized fixing the window error. A bounded debugger trace with
the X11 library loaded explicitly identifies the failing call in
`destroy_whole_window`, reached through `destroy_thread_windows` during
composition-worker thread teardown. The trace shows the host native window
destroyed on the main thread before the worker destroys its synthetic native
child. Normal lifetime tracing confirms that the child was parented to the
host. X11 parent destruction also destroys its native descendants. The later
child destroy therefore encounters `BadWindow`, which was finally delivered
while closing the worker's X11 connection. It is not a request to destroy
an arbitrary window identified from its screen position.

The source correction in `dlls/winex11.drv/window.c` uses Wine's existing
scoped X11 error-handling mechanism around native child cleanup. It accepts
only `BadWindow` for the specific `X_DestroyWindow` request and matching
resource identifier. Other errors are not ignored, and the global error
policy is unchanged. The extra synchronization occurs during destruction,
not per-frame rendering. This handles the legitimate case where the parent
has already destroyed the native child before its owning thread processes
the asynchronous Win32 teardown.

That change alone removed the fatal error in completed runs, but one of
three repetitions still stalled, matching the earlier baseline cleanup
problem. Source inspection found that our composition worker owns windows
but only sleeps between rendering passes. `dlls/dcomp/device.c` now processes
its message queue before acquiring the composition lock and waits with
`MsgWaitForMultipleObjects` rather than `Sleep`. This lets the window-owning
thread receive destruction requests without processing them under the
rendering lock. It does not change bitmap blending, shader behavior, or game
configuration. No message-processing change was made to the game itself.

The driver and composition module rebuilt without compiler warnings. After
both corrections, six consecutive normal runs completed 116 checks each,
without `BadWindow` or a timeout. Each retained the same three color failures
seen in the accepted installation. The final three runs took approximately
3.74, 3.84, and 3.84 seconds. The test assertions remain intact. These results
support the specific cleanup correction, not a claim that all composition
rendering tests pass or that SOA was visually verified.

The final bounded results are in
`test-results/safety/wine-child-cleanup-final-results-20261016.json`.
The debugger evidence is
`test-results/safety/wine-merge-x11-trace-preload-20261016.log`.
Both code changes are staged with the resolved merge. There are no unresolved
entries or unstaged source changes. No commit, push, installation, access
change, or game restart occurred. The accepted installed binaries retain
their prior hashes. The evidence establishes a native-child lifecycle
problem in the custom composition path, but does not isolate a single
official Wine commit as its cause.

### 2026-10-16: merge pushed and laptop handoff

The resolved update is now committed as one merge commit,
`d0f39775cdba5127093a6f61a273fb686c11c0e3`, and pushed to the public
`octaviothelatino/wine` repository on `dcomp-composition`. Its second parent
is official master `7b573fd7522d7e78957bdba1b175c717303923e8`.
No installation or game restart accompanied this source checkpoint.

The owner is moving to another laptop and authorized a private backup of
the accepted runtime and application files, excluding saved login and browser
state. The accepted installation remains associated with sanitized source
checkpoint `6f9a860511ccb7981777da050f2a93c3b53227bd`, not the new merge.
Its recorded composition and X11 module hashes were verified unchanged.

`LAPTOP-HANDOFF.md` records the source locations, checkpoints, private release,
restoration steps, build dependencies, and limitations. The stale overview
has been corrected. A minimal renderer registry file records Windows 10,
DreamSeeker OpenGL, and WebView2 Vulkan without exporting the full registry.
The ineffective buffer-sizing candidate is retained as an archived patch,
not active source. Compact final test evidence is retained with the notes.

Remaining work stays ordered: lighting, white tooltips, font appearance,
then HTML panel opening latency. Manual SOA acceptance on the new laptop
is still required. A verified archive is not evidence of a tested migration.
