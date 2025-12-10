// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/common/global_thread.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "shell/common/io_thread.h"

namespace lynxtron {
namespace {
GlobalThread* g_global_thread = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner() {
  return GlobalThread::GetUIThreadTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() {
  return GlobalThread::GetIOThreadTaskRunner();
}

GlobalThread::GlobalThread()
    : main_thread_task_executor_(
          std::make_unique<base::SingleThreadTaskExecutor>(
              base::MessagePumpType::UI,
              true)),
      io_thread_(std::make_unique<IOThread>()) {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;

  options.thread_type = base::ThreadType::kDisplayCritical;
  if (!io_thread_->StartWithOptions(std::move(options))) {
    LOG(FATAL) << "Failed to start IOThread";
  }

  task_runners_[ID::IO] = io_thread_->task_runner();
  task_runners_[ID::UI] = main_thread_task_executor_->task_runner();

  states_[ID::UI].store(BrowserThreadState::RUNNING, std::memory_order_relaxed);
  states_[ID::IO].store(BrowserThreadState::RUNNING, std::memory_order_relaxed);
}

GlobalThread::~GlobalThread() = default;

void GlobalThread::Create() {
  DCHECK(!g_global_thread);
  g_global_thread = new GlobalThread();
}

GlobalThread* GlobalThread::Get() {
  DCHECK(g_global_thread) << "No global thread created.";
  return g_global_thread;
}

// Callable on any thread.  Returns whether you're currently on a particular
// thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
bool GlobalThread::CurrentlyOn(ID identifier) {
  return Get()->task_runners_[identifier] &&
         Get()->task_runners_[identifier]->RunsTasksInCurrentSequence();
}

// static
bool GlobalThread::IsThreadInitialized(ID identifier) {
  return Get()->states_[identifier].load(std::memory_order_relaxed) ==
         BrowserThreadState::RUNNING;
}

scoped_refptr<base::SingleThreadTaskRunner>
GlobalThread::GetUIThreadTaskRunner() {
  return Get()->task_runners_[ID::UI];
}

scoped_refptr<base::SingleThreadTaskRunner>
GlobalThread::GetIOThreadTaskRunner() {
  return Get()->task_runners_[ID::IO];
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
GlobalThread::GetTaskRunnerForThread(ID identifier) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  switch (identifier) {
    case UI:
      return GetUIThreadTaskRunner();
    case IO:
      return GetIOThreadTaskRunner();
    case ID_COUNT:
      NOTREACHED();
  }
}

namespace internal {

bool BrowserThreadChecker::CalledOnValidBrowserThread(
    GlobalThread::ID thread_identifier) const {
  return GlobalThread::CurrentlyOn(thread_identifier);
}

const BrowserThreadChecker& GetBrowserThreadChecker(
    GlobalThread::ID thread_identifier) {
  static std::array<BrowserThreadChecker, GlobalThread::ID_COUNT>
      browser_thread_checkers;
  return browser_thread_checkers[thread_identifier];
}

#if DCHECK_IS_ON()

// static
std::string GetCurrentlyOnErrorMessage(GlobalThread::ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty()) {
    actual_name = "Unknown Thread";
  }
  // TODO(Guo Xi): organize this function

  std::string result = "Must be called on ";
  // result += BrowserThreadImpl::GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

ScopedValidateBrowserThreadDebugChecker::
    ScopedValidateBrowserThreadDebugChecker(
        GlobalThread::ID thread_identifier) {
  const auto& checker = GetBrowserThreadChecker(thread_identifier);
  DCHECK(checker.CalledOnValidBrowserThread(thread_identifier))
      << GetCurrentlyOnErrorMessage(thread_identifier);
}
#endif  // DCHECK_IS_ON()

}  // namespace internal
}  // namespace lynxtron
