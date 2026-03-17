// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "ui/gfx/image/buffer_w_stream.h"

#include <utility>

#include "base/compiler_specific.h"

namespace gfx {

BufferWStream::BufferWStream() = default;

BufferWStream::~BufferWStream() = default;

std::vector<uint8_t> BufferWStream::TakeBuffer() {
  return std::move(result_);
}

bool BufferWStream::write(const void* buffer, size_t size) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(buffer);
  result_.insert(result_.end(), bytes, UNSAFE_TODO(bytes + size));
  return true;
}

size_t BufferWStream::bytesWritten() const {
  return result_.size();
}

}  // namespace gfx
