// Copyright 2014 Cheng Zhao. All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/gin_helper/persistent_dictionary.h"

namespace gin_helper {

PersistentDictionary::PersistentDictionary() = default;

PersistentDictionary::PersistentDictionary(v8::Isolate* isolate,
                                           v8::Local<v8::Object> object)
    : isolate_(isolate), handle_(isolate, object) {}

PersistentDictionary::PersistentDictionary(const PersistentDictionary& other)
    : isolate_(other.isolate_),
      handle_(isolate_, v8::Local<v8::Object>::New(isolate_, other.handle_)) {}

PersistentDictionary::~PersistentDictionary() = default;

PersistentDictionary& PersistentDictionary::operator=(
    const PersistentDictionary& other) {
  isolate_ = other.isolate_;
  handle_.Reset(isolate_, v8::Local<v8::Object>::New(isolate_, other.handle_));
  return *this;
}

v8::Local<v8::Object> PersistentDictionary::GetHandle() const {
  return v8::Local<v8::Object>::New(isolate_, handle_);
}

}  // namespace gin_helper
