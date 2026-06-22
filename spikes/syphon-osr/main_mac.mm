// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 2 of 4).
//
// macOS browser-process entry point. Mirrors cefsimple/cefsimple_mac.mm.

#import <Cocoa/Cocoa.h>

#include "include/cef_app.h"
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"
#include "syphon_osr_app.h"

// Minimal NSApplication subclass required by CEF on macOS. Conforms to
// CefAppProtocol so CEF can track whether AppKit's send-event reentrancy guard
// is active. Mirrors cefsimple's SimpleApplication.
@interface SyphonOsrApplication : NSApplication <CefAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
@end

@implementation SyphonOsrApplication
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}
@end

// Entry point function for the browser process.
int main(int argc, char* argv[]) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(argc, argv);

  // Load the CEF framework library at runtime instead of linking directly, as
  // required on macOS.
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain()) {
    return 1;
  }

  @autoreleasepool {
    // Initialize the SyphonOsrApplication instance (NSApp).
    [SyphonOsrApplication sharedApplication];

    // CefApp for the browser process.
    CefRefPtr<SyphonOsrApp> app(new SyphonOsrApp());

    // Specify CEF global settings here.
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    // no_sandbox keeps the spike simple (no sandbox helper wiring). Fine for a
    // throwaway feasibility probe; not for production.
    settings.no_sandbox = true;
    // NOTE: multi_threaded_message_loop is unsupported on macOS — do not set it.

    // Initialize the CEF browser process. May return false if initialization
    // fails or if early exit is desired (for example, due to process singleton
    // relaunch behavior).
    if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
      return CefGetExitCode();
    }

    // Run the CEF message loop. This will block until CefQuitMessageLoop() is
    // called (from SyphonOsrHandler::OnBeforeClose).
    CefRunMessageLoop();

    // Shut down CEF.
    CefShutdown();
  }

  return 0;
}
