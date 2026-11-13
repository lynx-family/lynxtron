// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef UNIPLUGIN_UNIPLUGIN_GLOBALS_H_
#define UNIPLUGIN_UNIPLUGIN_GLOBALS_H_

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "uniplugin/plugin_service.h"
#include "uniplugin/process_callbacks.h"

namespace uniplugin {

class UnipluginGlobal {
 public:
  UnipluginGlobal() = default;
  ~UnipluginGlobal() = default;

  static UnipluginGlobal& GetInstance();
  void Start() {
    uniplugin_thread_ = std::make_unique<base::Thread>("uniplugin");
    uniplugin_thread_->Start();

    ipc_thread_ = std::make_unique<base::Thread>("ipc");
    ipc_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        ipc_thread_->task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }

  void Stop() {
    ipc_support_.reset();
    // ipc_thread_->Stop();
    ipc_thread_.reset();
    // uniplugin_thread_->Stop();
    uniplugin_thread_.reset();
    process_service_.reset();
    exit_manager_.reset();
  }

  void set_process_service(
      std::unique_ptr<uniplugin::PluginService> plugin_service) {
    process_service_ = std::move(plugin_service);
  }

  uniplugin::PluginService* process_service() { return process_service_.get(); }

  void set_process_callbacks(
      std::unique_ptr<ProcessCallbacks> process_callbacks) {
    process_callbacks_ = std::move(process_callbacks);
  }
  ProcessCallbacks* process_callbacks() { return process_callbacks_.get(); }

 public:
  std::unique_ptr<base::Thread> uniplugin_thread_;
  std::unique_ptr<base::Thread> ipc_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  std::unique_ptr<uniplugin::PluginService> process_service_;
  std::unique_ptr<base::AtExitManager> exit_manager_ =
      std::make_unique<base::AtExitManager>();
  std::unique_ptr<ProcessCallbacks> process_callbacks_ =
      std::make_unique<ProcessCallbacks>();
};

scoped_refptr<base::SingleThreadTaskRunner> uniplugin_task_runner();

}  // namespace uniplugin

#endif  // UNIPLUGIN_UNIPLUGIN_GLOBALS_H_
