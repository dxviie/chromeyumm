// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license.
//
// Throwaway macOS Syphon OSR feasibility spike (Step 3 of 4).
//
// Objective-C++ implementation of SyphonBridge. Isolates all Metal/Syphon/
// Objective-C state behind a pimpl so the rest of the spike stays pure C++.
//
// VERIFY: assumes this .mm is compiled with ARC (-fobjc-arc); Step 4 CMake must
// set that for this file. The code relies on ARC (no manual retain/release).

#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>
#import <Metal/Metal.h>

// Umbrella header from the built Syphon.framework. Pulls in
// SyphonServerBase.h + SyphonMetalServer.h.
#import "Syphon/Syphon.h"

#include "include/base/cef_logging.h"

#include "syphon_bridge.h"

struct SyphonBridge::Impl {
  id<MTLDevice> device;
  id<MTLCommandQueue> queue;
  SyphonMetalServer* server;
  bool started;
  bool failed;
  bool logged_first_frame;
  bool logged_texture_failure;
};

SyphonBridge::SyphonBridge() : impl_(new Impl{}) {}

SyphonBridge::~SyphonBridge() {
  Stop();
  delete impl_;
}

bool SyphonBridge::EnsureStarted(const char* server_name) {
  if (impl_->started) {
    return true;
  }
  if (impl_->failed) {
    return false;
  }

  impl_->device = MTLCreateSystemDefaultDevice();
  if (impl_->device == nil) {
    LOG(ERROR) << "[Q2] MTLCreateSystemDefaultDevice() returned nil — no Metal "
                  "device available. Syphon bridge disabled.";
    impl_->failed = true;
    return false;
  }

  impl_->queue = [impl_->device newCommandQueue];

  impl_->server = [[SyphonMetalServer alloc] initWithName:@(server_name)
                                                   device:impl_->device
                                                  options:nil];

  impl_->started = true;
  LOG(INFO) << "[Q2] Syphon server started: " << server_name;
  return true;
}

void SyphonBridge::Publish(IOSurfaceRef surface) {
  if (!impl_->started || surface == nullptr) {
    return;
  }

  const size_t w = IOSurfaceGetWidth(surface);
  const size_t h = IOSurfaceGetHeight(surface);

  MTLTextureDescriptor* desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                   width:w
                                  height:h
                               mipmapped:NO];
  desc.usage = MTLTextureUsageShaderRead;
  // VERIFY: on Apple Silicon MTLStorageModeShared is correct for an
  // IOSurface-backed texture; IOSurface-backed textures require a compatible
  // storage mode. If newTextureWithDescriptor:iosurface:plane: returns nil,
  // try MTLStorageModeManaged (Intel/discrete) instead.
  desc.storageMode = MTLStorageModeShared;

  id<MTLTexture> tex = [impl_->device newTextureWithDescriptor:desc
                                                     iosurface:surface
                                                         plane:0];
  if (tex == nil) {
    if (!impl_->logged_texture_failure) {
      impl_->logged_texture_failure = true;
      LOG(WARNING) << "[Q2] newTextureWithDescriptor:iosurface:plane: returned "
                      "nil — could not wrap IOSurface as a Metal texture. "
                      "Check pixel format / storage mode.";
    }
    return;
  }

  id<MTLCommandBuffer> cmd = [impl_->queue commandBuffer];

  // VERIFY: flipped:NO — if the Syphon receiver shows the image upside-down,
  // switch to flipped:YES (CEF/Chromium uses a top-left origin; Syphon/GL
  // convention is bottom-left).
  [impl_->server publishFrameTexture:tex
                     onCommandBuffer:cmd
                         imageRegion:NSMakeRect(0, 0, w, h)
                             flipped:NO];
  [cmd commit];

  if (!impl_->logged_first_frame) {
    impl_->logged_first_frame = true;
    LOG(INFO) << "[Q2] First frame published to Syphon (" << w << "x" << h
              << ").";
  }
}

void SyphonBridge::Stop() {
  if (impl_->server != nil) {
    [impl_->server stop];
    impl_->server = nil;
    LOG(INFO) << "[Q2] Syphon server stopped.";
  }
  impl_->queue = nil;
  impl_->device = nil;
  impl_->started = false;
}
