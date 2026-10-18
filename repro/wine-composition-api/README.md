# Wine D3D11/shared-surface API probe

This is the first, deterministic test in the Wine-fix workflow. It does not
launch BYOND, WebView2, or SOA, and it does not simulate flicker. It asks Wine
for the Windows graphics operations that the committed BYOND reproducer logs
as missing:

1. Create a D3D11 device and a shareable texture.
2. Query the DXGI resource, keyed mutex, and newer resource interfaces.
3. Request both legacy and NT shared handles.
4. Reopen any returned handles through D3D11.
5. Create a composition swap chain.
6. Create a DirectComposition device, visual, target, and commit the content
   when the swap chain exists.

The program prints every HRESULT. `E_NOTIMPL` means Wine reached the requested
Windows API but has not implemented that operation; `E_NOINTERFACE` means the
object does not expose the requested interface. These are API results, not
visual judgments.

## Build

The probe is a 32-bit Windows executable, matching BYOND 516:

```bash
cd /home/octavio/Downloads/linux-ports/repro/wine-composition-api
./build.sh
```

The default compiler is `i686-w64-mingw32-gcc`. Override it with `MINGW32_CC`
if needed.

## Run later in the control phase

The runner and prefix are deliberately supplied at run time so this probe does
not modify the working BYOND installation:

```bash
BYOND_PREFIX=/path/to/disposable-prefix \
BYOND_RUNNER=/path/to/wine-build/install \
./run.sh
```

This executable is the single-process API baseline. It is useful for seeing
which Windows calls return `E_NOTIMPL`, but it is not the final regression test
because it never crosses a process boundary. The authoritative first Wine test
will live in Wine's `dlls/d3d11/tests` and will create the texture in one
process, reopen it in a child process, and report the expected upstream failure
until the implementation is added. We are not running parallel runner
experiments while that test is being developed.
