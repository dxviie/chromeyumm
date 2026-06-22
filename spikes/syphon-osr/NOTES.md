# Grounded API Reference

Verbatim API facts the probe and bridge depend on, so later steps don't have to
re-derive them. Sourced from CEF 145/147 headers/docs, the Syphon-Framework
headers, and the Metal `MTLDevice` API.

## CEF — `CefRenderHandler::OnAcceleratedPaint`

```cpp
virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                PaintElementType type,
                                const RectList& dirtyRects,
                                const CefAcceleratedPaintInfo& info);
```

`cef_accelerated_paint_info_t` fields on **macOS**:

```c
size_t size;
cef_shared_texture_handle_t shared_texture_io_surface;  // macOS: IOSurface pointer / IOSurfaceRef
cef_color_type_t format;
cef_accelerated_paint_info_common_t extra;
```

On macOS, `shared_texture_io_surface` is the `IOSurfaceRef` (as a
`cef_shared_texture_handle_t`) we wrap as a Metal texture. The default `format` is
BGRA8.

### OSR setup requirements

- `CefWindowInfo::SetAsWindowless(...)` — create the browser as windowless (OSR).
- `CefBrowserSettings.windowless_frame_rate` — OSR target frame rate.
- The browser must be created with **windowless rendering** and the
  command-line / global setting `shared_texture_enabled` must be on (plus an
  ANGLE / Metal backend on macOS) for `OnAcceleratedPaint` to be used instead of
  the CPU `OnPaint` path.
- `GetViewRect` is a **required** `CefRenderHandler` override (it provides the OSR
  surface dimensions).

## Syphon — `SyphonMetalServer` (from `SyphonMetalServer.h`)

```objc
- (id)initWithName:(nullable NSString*)name
            device:(id<MTLDevice>)device
           options:(nullable NSDictionary<NSString *, id> *)options;

- (void)publishFrameTexture:(id<MTLTexture>)textureToPublish
              onCommandBuffer:(id<MTLCommandBuffer>)commandBuffer
                  imageRegion:(NSRect)region
                      flipped:(BOOL)isFlipped;

- (void)stop;
```

## Metal — wrap an IOSurface as a Metal texture (from `MTLDevice`)

```objc
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor*)descriptor
                                  iosurface:(IOSurfaceRef)iosurface
                                      plane:(NSUInteger)plane;
```

- Descriptor `pixelFormat`: **BGRA8Unorm** (CEF default).
- Descriptor `width` / `height`: from `IOSurfaceGetWidth(...)` /
  `IOSurfaceGetHeight(...)`.
- Descriptor `usage`: `MTLTextureUsageShaderRead`.
- `plane`: `0` (single-plane BGRA surface).

## Frameworks to link on macOS

- Metal
- IOSurface
- Foundation / Cocoa
- CoreVideo
- Syphon.framework
