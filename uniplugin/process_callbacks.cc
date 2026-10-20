// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "uniplugin/process_callbacks.h"

namespace uniplugin {

ProcessCallbacks::ProcessCallbacks() = default;
ProcessCallbacks::~ProcessCallbacks() = default;
void ProcessCallbacks::set_message_callback(
    UPMessagePortMessageCallback callback) {
  if (callback) {
    message_callback_ = callback;
  }
}

void ProcessCallbacks::set_spawn_callback(
    UPPluginProcessSpawnCallback callback) {
  if (callback) {
    spawn_callback_ = callback;
  }
}

void ProcessCallbacks::set_exit_callback(UPChildProcessCloseCallback callback) {
  if (callback) {
    exit_callback_ = callback;
  }
}
UPMessagePortMessageCallback ProcessCallbacks::message_callback() {
  return message_callback_;
}
UPPluginProcessSpawnCallback ProcessCallbacks::spawn_callback() {
  return spawn_callback_;
}

UPChildProcessCloseCallback ProcessCallbacks::exit_callback() {
  return exit_callback_;
}

}  // namespace uniplugin
