// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 2 of 4).
//
// macOS helper (sub-process) entry point. CEF spawns separate executables for
// its GPU / renderer / utility / plugin processes; this single binary serves
// them all.
//
// In Step 4 this is built into the executable placed inside each
// `<app>.app/Contents/Frameworks/<app> Helper*.app/Contents/MacOS/` bundle
// (one bundle per helper variant). Mirrors cefsimple's process_helper_mac.cc.

#include "include/cef_app.h"
#include "include/wrapper/cef_library_loader.h"

// Entry point function for sub-processes.
int main(int argc, char* argv[]) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(argc, argv);

  // Load the CEF framework library at runtime instead of linking directly, as
  // required on macOS. Use the helper variant of the loader.
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInHelper()) {
    return 1;
  }

  // Execute the sub-process. No CefApp is required for these helper processes
  // in this spike (the OSR/command-line handling lives in the browser process).
  return CefExecuteProcess(main_args, nullptr, nullptr);
}
