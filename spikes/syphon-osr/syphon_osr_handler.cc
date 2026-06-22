// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 2 of 4).

#include "syphon_osr_handler.h"

// IOSurface's C API is usable directly from C++ (Objective-C++ not required for
// the probe itself). Step 3's Syphon/Metal bridge will be Objective-C++.
#include <IOSurface/IOSurface.h>

#include "include/base/cef_logging.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"

namespace {

// Log the first accelerated frame, then roughly once per second at 60fps so the
// console isn't spammed.
constexpr uint64_t kLogEveryNFrames = 60;

}  // namespace

SyphonOsrHandler::SyphonOsrHandler()
    : syphon_bridge_(std::make_unique<SyphonBridge>()) {}

SyphonOsrHandler::~SyphonOsrHandler() = default;

void SyphonOsrHandler::GetViewRect(CefRefPtr<CefBrowser> browser,
                                   CefRect& rect) {
  // Required CefRenderHandler override: provides the OSR surface dimensions.
  rect.Set(0, 0, 1280, 720);
}

void SyphonOsrHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                               PaintElementType type,
                               const RectList& dirtyRects,
                               const void* buffer,
                               int width,
                               int height) {
  CEF_REQUIRE_UI_THREAD();
  // The CPU fallback path. If this fires, accelerated paint is NOT active and
  // Q1 is trending to NO. Log once so we don't flood the console.
  if (!logged_onpaint_warning_) {
    logged_onpaint_warning_ = true;
    LOG(WARNING) << "[Q1] OnPaint called (" << width << "x" << height
                 << ") — accelerated paint NOT active (Q1 = NO so far). "
                 << "Check that off-screen-rendering-enabled + "
                 << "shared-texture-enabled + windowless_rendering_enabled "
                 << "are all set.";
  }
}

void SyphonOsrHandler::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  CEF_REQUIRE_UI_THREAD();

  const uint64_t frame = accelerated_frame_count_++;
  const bool should_log = (frame == 0) || (frame % kLogEveryNFrames == 0);

  // info.shared_texture_io_surface is the macOS IOSurface handle (an
  // IOSurfaceRef stored as a cef_shared_texture_handle_t). Non-null here is the
  // core positive signal for Q1.
  // VERIFY: field name is `shared_texture_io_surface` on macOS per NOTES.md /
  // cef_accelerated_paint_info_t. On Windows the field is `shared_texture_handle`.
  const bool has_iosurface = (info.shared_texture_io_surface != nullptr);

  if (should_log) {
    LOG(INFO) << "[Q1] OnAcceleratedPaint fired"
              << " frame=" << frame << " type=" << static_cast<int>(type)
              << " format=" << static_cast<int>(info.format)
              << " dirtyRects=" << dirtyRects.size()
              << " iosurface=" << (has_iosurface ? "NON-NULL" : "NULL");
  }

  if (has_iosurface) {
    IOSurfaceRef io_surface =
        reinterpret_cast<IOSurfaceRef>(info.shared_texture_io_surface);
    if (should_log) {
      const size_t w = IOSurfaceGetWidth(io_surface);
      const size_t h = IOSurfaceGetHeight(io_surface);
      const OSType pixel_format = IOSurfaceGetPixelFormat(io_surface);
      // Decode the FourCC pixel format into a readable tag for the console.
      const char fourcc[5] = {
          static_cast<char>((pixel_format >> 24) & 0xff),
          static_cast<char>((pixel_format >> 16) & 0xff),
          static_cast<char>((pixel_format >> 8) & 0xff),
          static_cast<char>((pixel_format) & 0xff), '\0'};
      LOG(INFO) << "[Q1] IOSurface OK: " << w << "x" << h
                << " pixelFormat=0x" << std::hex << pixel_format << std::dec
                << " ('" << fourcc << "') — Q1 = YES.";
    }

    // Step 3: hand this IOSurfaceRef to the Syphon bridge. The bridge wraps it
    // as a Metal texture (newTextureWithDescriptor:iosurface:plane:) and
    // publishes via SyphonMetalServer::publishFrameTexture:... See NOTES.md.
    // Called on EVERY accelerated frame (not gated by should_log); the bridge
    // lazily initialises on the first call and no-ops if Metal/Syphon fail.
    syphon_bridge_->EnsureStarted("Chromeyumm OSR Spike");
    syphon_bridge_->Publish(io_surface);
  } else if (should_log) {
    LOG(WARNING) << "[Q1] OnAcceleratedPaint fired but "
                    "shared_texture_io_surface is NULL — shared textures not "
                    "actually backing the surface (Q1 = NO).";
  }
}

void SyphonOsrHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  base::AutoLock lock_scope(lock_);
  // Single-browser spike: keep the one browser reference.
  browser_ = browser;
  LOG(INFO) << "[spike] OSR browser created.";
}

bool SyphonOsrHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  // Allow default close behavior to proceed.
  return false;
}

void SyphonOsrHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  // Tear down the Syphon server before dropping the browser reference.
  syphon_bridge_->Stop();
  {
    base::AutoLock lock_scope(lock_);
    browser_ = nullptr;
  }
  // Single browser: quit the message loop when it closes.
  CefQuitMessageLoop();
}
