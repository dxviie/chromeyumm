// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 2 of 4).

#ifndef CEF_SPIKES_SYPHON_OSR_SYPHON_OSR_HANDLER_H_
#define CEF_SPIKES_SYPHON_OSR_SYPHON_OSR_HANDLER_H_

#include <memory>

#include "include/base/cef_lock.h"
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_render_handler.h"

#include "syphon_bridge.h"

// Single-browser CefClient that implements the offscreen render handler.
// Its job in Step 2 is purely diagnostic: report whether OnAcceleratedPaint
// fires on macOS and whether info.shared_texture_io_surface is a non-null
// IOSurfaceRef. Step 3 extends OnAcceleratedPaint to publish that IOSurface
// through Syphon.
class SyphonOsrHandler : public CefClient,
                         public CefRenderHandler,
                         public CefLifeSpanHandler {
 public:
  SyphonOsrHandler();
  ~SyphonOsrHandler() override;

  // CefClient methods:
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

  // CefRenderHandler methods:
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override;
  void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          PaintElementType type,
                          const RectList& dirtyRects,
                          const CefAcceleratedPaintInfo& info) override;

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

 private:
  // The single OSR browser. Touched on the UI thread only, but guarded for
  // safety mirroring cefsimple.
  CefRefPtr<CefBrowser> browser_;
  base::Lock lock_;

  // Diagnostic counters / one-shot guards (UI-thread only).
  uint64_t accelerated_frame_count_ = 0;
  bool logged_onpaint_warning_ = false;

  // Step 3: IOSurface -> Metal texture -> SyphonMetalServer bridge. The
  // Objective-C++/Metal/Syphon state is hidden behind this pimpl interface.
  std::unique_ptr<SyphonBridge> syphon_bridge_;

  IMPLEMENT_REFCOUNTING(SyphonOsrHandler);
  DISALLOW_COPY_AND_ASSIGN(SyphonOsrHandler);
};

#endif  // CEF_SPIKES_SYPHON_OSR_SYPHON_OSR_HANDLER_H_
