# BYOND and WebView2 on Wine

Start with [the laptop handoff](LAPTOP-HANDOFF.md) for the current checkpoints,
private runtime backup, restoration instructions, and remaining issues.

This private repository contains configuration, investigation notes, and test
helpers. Wine source lives separately in the public repository
[octaviothelatino's Wine repository](https://github.com/octaviothelatino/wine).

The intended layout has one source tree, one build, and one isolated game prefix:

```text
Downloads/
  wine-upstream/
  wine-build-upstream32/
    install/
  linux-ports/
    byond32-upstream-baseline/
```

The name `byond32-upstream-baseline` is historical. Its accepted runner contains
our composition changes. It is not an unmodified official Wine build.

The latest source merges official master into `dcomp-composition`. The installed
runner remains the earlier manually accepted checkpoint. A source update is not
an installed or manually tested game update.

Normal play uses `tools/play-soa.sh`. Diagnostics use `tools/byond-test-runner.sh`.
Generated output and the game prefix are ignored by Git. The private runtime
release is separate from source commits.

The real DreamSeeker project is in `repro/byond-browser-control`. The standalone
WebView2 project in `repro/dreamseeker-webview2` is a diagnostic aid, not a proven
equivalent of SOA's bug. Real SOA manual verification remains authoritative.

The dated history and roadmap are in
[the investigation diary](BYOND-WINE-WEBVIEW2-INVESTIGATION.md).
The Lutris installer files in `config` are historical. Their Windows 7 setting
and browser GPU-disabling arguments are not the current accepted configuration.
