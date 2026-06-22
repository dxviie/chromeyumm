// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 2 of 4).

#ifndef CEF_SPIKES_SYPHON_OSR_SYPHON_OSR_APP_H_
#define CEF_SPIKES_SYPHON_OSR_SYPHON_OSR_APP_H_

#include "include/cef_app.h"

// CefApp for the browser process. Enables OSR + shared textures via the
// command line and creates the single windowless (offscreen) browser used to
// probe OnAcceleratedPaint.
class SyphonOsrApp : public CefApp, public CefBrowserProcessHandler {
 public:
  SyphonOsrApp();

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) override;

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override;

 private:
  IMPLEMENT_REFCOUNTING(SyphonOsrApp);
};

#endif  // CEF_SPIKES_SYPHON_OSR_SYPHON_OSR_APP_H_
