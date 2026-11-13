// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef UNIPLUGIN_PROCESS_CALLBACKS_H_
#define UNIPLUGIN_PROCESS_CALLBACKS_H_

#include "uniplugin/public/uniplugin.h"

namespace uniplugin {

class ProcessCallbacks {
 public:
  ProcessCallbacks();
  ~ProcessCallbacks();
  void set_message_callback(UPMessagePortMessageCallback callback);
  void set_spawn_callback(UPPluginProcessSpawnCallback callback);
  void set_exit_callback(UPChildProcessCloseCallback callback);
  UPMessagePortMessageCallback message_callback();
  UPPluginProcessSpawnCallback spawn_callback();
  UPChildProcessCloseCallback exit_callback();

 private:
  UPMessagePortMessageCallback message_callback_{};
  UPPluginProcessSpawnCallback spawn_callback_{};
  UPChildProcessCloseCallback exit_callback_{};
};
}  // namespace uniplugin

#endif  // UNIPLUGIN_PROCESS_CALLBACKS_H_
