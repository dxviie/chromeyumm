# Syphon macOS OSR Feasibility Spike

A throwaway feasibility spike. This is the sibling experiment to the design doc
at [`docs/design-docs/syphon-mac-output.md`](../../docs/design-docs/syphon-mac-output.md).
It is disposable: its only purpose is to de-risk the macOS Syphon port before we
commit to it. Do not treat this code as production-bound.

## Goal

Answer one risky question and prove one chain end-to-end:

- **Q1:** Does the CEF build we target (CEF 147 / Chromium 147) actually invoke
  `CefRenderHandler::OnAcceleratedPaint` on macOS with a non-null
  `shared_texture_io_surface` (an `IOSurfaceRef`), when offscreen rendering (OSR)
  + shared textures are enabled?
- **Q2:** Can we wrap that `IOSurface` as a Metal texture and publish it through a
  `SyphonMetalServer` so it shows up live in a Syphon receiver (e.g. Syphon's
  "Simple Client")?

If both are yes, the GPU-native zero-copy path (Option A in the design doc) is
viable on macOS, mirroring what Spout gives us on Windows.

## Prerequisites

- **macOS** (Apple Silicon or Intel) with a Metal-capable GPU.
- **Xcode** + command-line tools (`xcode-select --install`).
- **CMake** (3.21+ recommended).
- **CEF 147 macOS 64-bit "Standard Distribution"** binary from
  <https://cef-builds.spotifycdn.com/index.html>. The repo pins
  `147.0.9+g2812b73+chromium-147.0.7727.49` in
  [`scripts/setup-vendors.ts`](../../scripts/setup-vendors.ts) — download the
  **macOS** build of that same version (the Standard Distribution, which includes
  the `tests/` sample sources and the CMake machinery).
- **Syphon-Framework** — <https://github.com/Syphon/Syphon-Framework> — built into
  `Syphon.framework`.
- **Syphon "Simple Client"** receiver app, for visual confirmation that frames are
  actually being published.

## Layout

This spike is designed to be **dropped into the unpacked CEF macOS distribution's
`tests/` directory** and added to the distro's top-level `CMakeLists.txt` via:

```cmake
add_subdirectory(syphon-osr)
```

This deliberately mirrors how the bundled `cefsimple` sample is structured, so we
reuse CEF's app-bundle + helper-process bundling machinery. That bundling is fiddly
and version-specific on macOS (main app bundle, multiple helper bundles for the
GPU/renderer/plugin sub-processes, framework copy/sign steps), so cloning the
`cefsimple` pattern is far safer than hand-rolling it. The concrete wiring lives in
this directory's `CMakeLists.txt` and `mac/*.plist`; see **Build & run** below.

## Build & run

All steps run **on a Mac** — this spike cannot be compiled on Linux.

1. **Download + unpack the CEF distribution.** Get the **CEF 147 macOS 64-bit
   "Standard Distribution"** (same version pinned in
   [`scripts/setup-vendors.ts`](../../scripts/setup-vendors.ts)) from
   <https://cef-builds.spotifycdn.com/index.html> and unpack it, e.g. to
   `~/cef-distro`. The Standard Distribution includes `tests/cefsimple`, the
   `cmake/` macros, and the CEF framework.
2. **Build `Syphon.framework`.** Clone
   <https://github.com/Syphon/Syphon-Framework>, build the framework target in
   Xcode (Release), and note the output directory that *contains*
   `Syphon.framework` (e.g. `~/Syphon-Framework/build/Release`).
3. **Copy this spike into the distro.** `cp -R` (or symlink) this
   `spikes/syphon-osr` directory to `<cef-distro>/tests/syphon-osr`:
   ```sh
   cp -R spikes/syphon-osr ~/cef-distro/tests/syphon-osr
   ```
4. **Wire it into the distro build.** Add the spike next to the existing
   cefsimple line in `<cef-distro>/CMakeLists.txt`:
   ```cmake
   add_subdirectory(tests/cefsimple)
   add_subdirectory(tests/syphon-osr)   # <-- add this
   ```
5. **Configure + build.** From a build dir inside the distro, point CMake at the
   Syphon framework dir from step 2. Build the `syphon_osr` target.
   ```sh
   cd ~/cef-distro && mkdir -p build && cd build
   # Apple Silicon (arm64):
   cmake -G "Xcode" -DPROJECT_ARCH=arm64 \
     -DSYPHON_FRAMEWORK_DIR=~/Syphon-Framework/build/Release ..
   cmake --build . --target syphon_osr --config Release
   ```
   For Intel use `-DPROJECT_ARCH=x86_64`. A Ninja generator
   (`-G Ninja`) works too. The `syphon_osr` app bundle lands under
   `build/tests/syphon-osr/Release/Syphon OSR Spike.app` (path depends on
   generator/config).
6. **Run + watch the console.** Launch `Syphon OSR Spike.app` (from Finder, or
   `./.../Syphon\ OSR\ Spike.app/Contents/MacOS/syphon_osr` for inline stdout).
   Watch the console/stdout for the probe's `[Q1]` log lines (does
   `OnAcceleratedPaint` fire with a non-null `IOSurface`?) and the bridge's
   `[Q2]` log lines (is the Metal texture wrapped and published?).
7. **Confirm in a Syphon receiver.** Open Syphon's **"Simple Client"** and
   confirm a server named **"Chromeyumm OSR Spike"** appears showing the
   spinning test page. That is the Q2 = YES signal.

> **Build note:** `CMakeLists.txt` links `Syphon.framework` plus the `Metal`,
> `IOSurface`, and `CoreVideo` frameworks, and compiles `syphon_bridge.mm` with
> ARC (`-fobjc-arc`). It also needs `-DSYPHON_FRAMEWORK_DIR` (step 5) and will
> fail configuration with a clear message if that is unset.

**Decision gate** once the spike runs:

- If `OnAcceleratedPaint` fires with a **non-null `IOSurface`**, then **Option A**
  (GPU-native zero-copy) in the design doc is viable — proceed with the port.
- If CEF only ever calls **`OnPaint`**, or the `IOSurface` is **null**, fall back to
  **Option B** (CPU `BgraFrameView` upload) — or bump/patch CEF to a build that
  supports accelerated paint on macOS.

## Status checklist

- [x] Step 1 — Scaffold + docs (this directory: `README.md`, `NOTES.md`).
- [x] Step 2 — OSR probe (`OnAcceleratedPaint` / IOSurface logging).
- [x] Step 3 — Syphon bridge (IOSurface → Metal → `SyphonMetalServer`).
- [x] Step 4 — Build glue (CMake target + macOS bundle wiring).

## Cleanup

This is a **throwaway spike**. Once Q1/Q2 are answered, record the result in
[`docs/design-docs/syphon-mac-output.md`](../../docs/design-docs/syphon-mac-output.md)
(the "Open Questions / Risks" / Decision sections) and delete
`spikes/syphon-osr/`.
