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
`cefsimple` pattern is far safer than hand-rolling it.

> The exact CMake wiring (target, framework links, helper bundles, Info.plist) lands
> in **Step 4**. Nothing in this directory is wired into the CEF build yet.

## Build & run / Decision gate

> Placeholder — filled in by later steps:
> - **Step 2** adds the OSR probe (a `CefRenderHandler` that logs whether
>   `OnAcceleratedPaint` fires and whether the `IOSurface` is non-null).
>
> The probe logs IOSurface presence, size, and pixel format via `LOG(INFO)`
> when `OnAcceleratedPaint` fires, and warns loudly via `LOG(WARNING)` if the
> CPU `OnPaint` path is hit instead (the Q1 = NO signal).
> - **Step 3** adds the Syphon bridge (IOSurface → Metal texture →
>   `SyphonMetalServer`).
> - **Step 4** adds the build glue (CMake target + bundle wiring).

Once Q1 passes (accelerated paint with a non-null IOSurface), every accelerated
frame is wrapped as a Metal texture and published to a Syphon server named
**"Chromeyumm OSR Spike"** — open Syphon's **"Simple Client"** receiver to
confirm the live frames appear there (that is the Q2 = YES signal).

> **Step 4 build note:** the CMake target must link `Syphon.framework` plus the
> `Metal`, `IOSurface`, and `CoreVideo` frameworks, and must compile
> `syphon_bridge.mm` with ARC (`-fobjc-arc`).

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
- [ ] Step 4 — Build glue (CMake target + macOS bundle wiring).
