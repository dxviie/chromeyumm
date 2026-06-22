// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 2 of 4).

#include "syphon_osr_app.h"

#include <string>

#include "include/base/cef_logging.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "syphon_osr_handler.h"

SyphonOsrApp::SyphonOsrApp() = default;

void SyphonOsrApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  // These two switches, together with CefSettings.windowless_rendering_enabled
  // (set in main_mac.mm), are what gate OnAcceleratedPaint on macOS. Without
  // shared textures enabled, CEF falls back to the CPU OnPaint path and
  // info.shared_texture_io_surface is never populated.
  //
  // Apply only to the browser process (empty process_type) so we don't perturb
  // the helper sub-processes.
  if (process_type.empty()) {
    command_line->AppendSwitch("off-screen-rendering-enabled");
    command_line->AppendSwitch("shared-texture-enabled");
  }
}

void SyphonOsrApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<SyphonOsrHandler> handler(new SyphonOsrHandler());

  // Windowless (offscreen) rendering. No parent window on macOS.
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);

  CefBrowserSettings browser_settings;
  // OSR target frame rate.
  browser_settings.windowless_frame_rate = 60;

  // Self-contained animated page: a CSS @keyframes-driven box keeps the
  // compositor producing frames, so OnAcceleratedPaint fires continuously.
  const std::string url =
      "data:text/html,"
      "<html><head><style>"
      "html,body{margin:0;height:100%25;background:%23101820;overflow:hidden}"
      "@keyframes spin{from{transform:rotate(0deg) translateX(160px)}"
      "to{transform:rotate(360deg) translateX(160px)}}"
      ".box{position:absolute;top:50%25;left:50%25;width:120px;height:120px;"
      "margin:-60px;background:linear-gradient(45deg,%23ff4081,%2300e5ff);"
      "animation:spin 2s linear infinite}"
      "</style></head><body><div class='box'></div></body></html>";

  CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings,
                                nullptr, nullptr);
}
