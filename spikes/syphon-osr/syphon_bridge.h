// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 3 of 4).
//
// Plain C++ interface to the Syphon/Metal bridge. The implementation is
// Objective-C++ and lives in syphon_bridge.mm; this header is deliberately
// free of any Objective-C types so the rest of the spike stays pure C++.
// IOSurfaceRef is fine here — it is a CoreFoundation/C type.

#ifndef CEF_SPIKES_SYPHON_OSR_SYPHON_BRIDGE_H_
#define CEF_SPIKES_SYPHON_OSR_SYPHON_BRIDGE_H_

#include <IOSurface/IOSurface.h>

// Wraps a CEF IOSurface as a Metal texture and publishes it through a
// SyphonMetalServer. The Objective-C++/Metal/Syphon state is hidden behind a
// pimpl so this header (and its includers) remain plain C++.
class SyphonBridge {
 public:
  SyphonBridge();
  ~SyphonBridge();

  // Lazily creates the Metal device + SyphonMetalServer. Safe to call every
  // frame; only the first call does work. Returns false if Metal/Syphon
  // could not be initialised (bridge then no-ops).
  bool EnsureStarted(const char* server_name);

  // Wraps the IOSurface as a BGRA8 Metal texture and publishes one frame.
  void Publish(IOSurfaceRef surface);

  void Stop();

 private:
  struct Impl;
  Impl* impl_;  // pimpl — hides id<MTLDevice>, SyphonMetalServer*, etc.
};

#endif  // CEF_SPIKES_SYPHON_OSR_SYPHON_BRIDGE_H_
