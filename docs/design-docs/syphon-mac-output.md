# Decision: Syphon (macOS) as a Spout Alternative

## Status

Proposed (feasibility investigation — no implementation yet)

## Context

Chromeyumm sends the rendered browser canvas to other AV software (TouchDesigner,
Resolume, OBS, MadMapper…) via **Spout**. Spout is **Windows + Direct3D 11 only**.
On macOS the de-facto equivalent is **[Syphon](http://syphon.info/)**: a GPU
texture-sharing framework built on IOSurface + Metal/OpenGL. This doc investigates
adding a Syphon *output* as the macOS counterpart to the Spout sender, keeping the
project's "one browser, many outputs / GPU-first, CPU-never" pipeline intact.

The headline finding: **Spout only *looks* like a pluggable protocol.** It is
implemented as `SpoutOutput : IOutputProtocol` (`native/frame-output/protocols/spout/spout_output.{h,cpp}`),
which suggests a sibling `SyphonOutput` would slot in cleanly. In reality the
entire layer underneath that interface is hard-bound to Windows + D3D11, and the
project is **Windows-only by explicit design decree**
(`docs/DESIGN.md` — *"No cross-platform guards. This is Windows-only."*). Adding
Syphon is therefore a **macOS native-backend port**, not a plugin. The
`SyphonOutput` class is the small part.

Concrete coupling points found in the codebase:

- **The GPU frame view *is* a D3D11 texture.**
  `struct GpuFrameView { ID3D11Texture2D* texture; }`
  (`native/frame-output/core/frame_types.h`). Every GPU-path output
  (`IOutputProtocol::OnGpuFrame`) receives a raw `ID3D11Texture2D*`.
- **The runtime entry point is DXGI-specific.**
  `ProcessSharedFrame(webviewId, ID3D11Device*, ID3D11DeviceContext*, ID3D11Texture2D*)`
  (`native/frame-output/frame_transport_runtime.h`), fed from
  `OnAcceleratedPaint` in `native/cef-wrapper.cpp`, which opens a DXGI NT shared
  handle on the app's D3D11 device.
- **The frame source differs per platform.** CEF OSR runs ANGLE-d3d11 and
  delivers a DXGI shared-texture handle on Windows. On macOS the corresponding
  `OnAcceleratedPaint` payload is an **`IOSurfaceRef`**
  (`cef_accelerated_paint_info_t.shared_texture_io_surface`). In *stock* CEF
  builds `OnAcceleratedPaint` is **Windows-only** (macOS falls back to `OnPaint`);
  accelerated/shared-texture OSR on macOS is patch- and version-dependent and has
  historically been fragile. **This is the single largest risk.**
- **The build system is MSVC-only.** `build.ts` drives `cl.exe` / `link.exe` via
  `vcvarsall.bat`; there is no clang/Xcode path. `scripts/setup-vendors.ts`
  fetches CEF (Windows 64-bit) and the Spout2 source.
- **No macOS / Metal / IOSurface code exists** anywhere in the native layer today.
- **Syphon's publish API is Objective-C / Metal:**
  `SyphonMetalServer publishFrameTexture:onCommandBuffer:imageRegion:flipped:`.
  It wants an `id<MTLTexture>` — ideally one wrapping the very IOSurface that CEF
  hands us, so no pixels are copied.

## Options Considered

### A. GPU-native, zero-copy (chosen)

macOS CEF OSR delivers an `IOSurfaceRef` via `OnAcceleratedPaint`. Wrap that
surface as a Metal texture with
`-[MTLDevice newTextureWithDescriptor:iosurface:plane:]` and hand it straight to
`SyphonMetalServer publishFrameTexture:…`. No CPU readback, no PCIe round-trip —
a true mirror of Spout's `SendTexture` GPU path and of DESIGN.md principle #2
("GPU-first, CPU-never").

- **Pro:** True Spout parity; constant cost; honors the core design principle.
- **Pro:** IOSurface → Metal wrapping is zero-copy; Syphon itself is built for
  exactly this.
- **Con:** Depends on macOS CEF firing `OnAcceleratedPaint` with a usable
  IOSurface — may require a newer/patched CEF.
- **Con:** Requires a Metal device + Obj-C++ runtime living alongside CEF, and a
  whole new clang/Xcode build path.

### B. CPU-fallback (BgraFrameView upload)

Reuse the existing CPU path (`IOutputProtocol::NeedsCpuFrame` / `OnFrame` with a
`BgraFrameView`), upload BGRA into a Metal texture each frame, then publish via
Syphon.

- **Pro:** Far simpler; works even if macOS accelerated paint is unavailable
  (plain `OnPaint` gives CPU pixels).
- **Con:** Two PCIe crossings per frame; violates the zero-copy rule. Suitable
  only as a fallback if Option A is blocked by CEF.

### C. NDI instead of Syphon

NDI is cross-platform and would serve Windows *and* macOS. Noted for completeness
but **out of scope** here — the request is specifically a Spout-equivalent on the
Mac, and NDI is network-framed (compression/latency) rather than local zero-copy
GPU sharing.

## Decision

**Option A — GPU-native, zero-copy Syphon output.** Fall back to Option B only if
the target CEF build cannot deliver an IOSurface through `OnAcceleratedPaint` on
macOS.

## Consequences / Implementation Sketch

This is a design sketch naming the seams a future implementation touches — not a
committed change.

- **Make `GpuFrameView` platform-typed.** Today it is `ID3D11Texture2D*`. Keep the
  interface protocol-agnostic by carrying a platform-typed backing handle —
  `ID3D11Texture2D*` on Windows, `IOSurfaceRef` (or `id<MTLTexture>`) on macOS —
  so `IOutputProtocol::OnGpuFrame` is unchanged for all outputs.
  (`native/frame-output/core/frame_types.h`, `core/output_protocol.h`.)
- **macOS frame entry point.** A counterpart to `ProcessSharedFrame()` that
  accepts the IOSurface from `OnAcceleratedPaint` instead of a DXGI handle.
  (`native/frame-output/frame_transport_runtime.{h,cpp}` and the
  `OnAcceleratedPaint` site in `native/cef-wrapper.cpp`.)
- **New protocol class.** `native/frame-output/protocols/syphon/syphon_output.{h,mm}`
  implementing `IOutputProtocol`, mirroring `protocols/spout/spout_output.{h,cpp}`.
  Use Obj-C++ (`.mm`) because Syphon is Objective-C. Guard it exactly like Spout
  guards `CHROMEYUMM_HAS_SPOUT`, e.g. `#if __has_include(<Syphon/Syphon.h>)`, so a
  build without the framework degrades to a no-op `Start() → false`.
- **Per-webview Metal device.** A long-lived `MTLDevice` (parallel to the D3D11
  device held in `FrameOutputHostState`), used both to wrap the IOSurface and to
  construct the `SyphonMetalServer`.
- **FFI + exports.** `startSyphonSender` / `stopSyphonSender` mirroring
  `startSpoutSender` / `stopSpoutSender` in
  `native/frame-output/frame_transport_exports.cpp` and `native/exports.def`;
  Bun FFI symbols in `src/chromeyumm/ffi.ts`; a `BrowserWindow.startSyphon()`
  wrapper in `src/chromeyumm/browser-window.ts`; startup wiring in
  `src/app/index.ts`; a `SyphonOutputConfig` type in `src/app/config.ts` so
  `display-config.json` can carry `syphonOutput.senderName`.
- **Build path (largest net-new piece).** A macOS branch in `build.ts` using
  clang / `xcrun` instead of MSVC, linking `Syphon.framework` plus `Metal`,
  `IOSurface`, and `Foundation`; and a Syphon-Framework vendor fetch in
  `scripts/setup-vendors.ts`.

## Open Questions / Risks

1. **CEF macOS accelerated paint.** Does the CEF build Chromeyumm targets actually
   invoke `OnAcceleratedPaint` (with a usable `shared_texture_io_surface`) on
   macOS, or is a newer/patched CEF required? This gates Option A and must be
   answered with a spike before implementation. A runnable spike that answers
   this end-to-end lives in `spikes/syphon-osr/` (see its README).
2. **Pixel format / orientation / color.** BGRA vs sRGB, and the `flipped:`
   argument to `publishFrameTexture:` — must match what Spout produces so existing
   receivers look identical.
3. **Project policy.** DESIGN.md currently forbids cross-platform guards
   ("Windows-only"). Adopting macOS means either relaxing that rule or partitioning
   mac code behind a compile-time `__APPLE__` boundary so the Windows build stays
   branch-free. This is a team decision, not just a code decision.

## Effort Estimate

- **This design doc:** small.
- **Implementation:** large / multi-week. It is effectively a second-platform
  native port — CEF-for-mac build, Metal device bootstrap, IOSurface
  accelerated-paint plumbing, and a clang/Xcode build path — with `SyphonOutput`
  itself being a thin wrapper. Highest-risk dependency is macOS CEF
  shared-texture OSR (open question #1).

## References

- Syphon-Framework — `SyphonMetalServer`
  (`publishFrameTexture:onCommandBuffer:imageRegion:flipped:`):
  https://github.com/Syphon/Syphon-Framework
- CEF `cef_accelerated_paint_info_t` (incl. `shared_texture_io_surface`):
  https://cef-builds.spotifycdn.com/docs/132.3/structcef__accelerated__paint__info__t.html
- Related in-repo docs: [osr-shared-texture.md](osr-shared-texture.md),
  [single-master-gpu-blit.md](single-master-gpu-blit.md),
  [spout-input-shared-memory.md](spout-input-shared-memory.md)
