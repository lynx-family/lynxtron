// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/common/global_thread.h"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "shell/common/io_thread.h"

namespace lynxtron {
namespace {

struct GlobalThreadGlobals {
  GlobalThreadGlobals() {
    // GlobalThreadGlobals must be initialized on main thread before it's used
    // by any other threads.
    DETACH_FROM_THREAD(main_thread_checker_);
  }

  THREAD_CHECKER(main_thread_checker_);

  base::Lock lock;

  std::array<scoped_refptr<base::SingleThreadTaskRunner>,
             GlobalThread::ID_COUNT>
      task_runners GUARDED_BY(lock);

  std::array<GlobalThread::State, GlobalThread::ID_COUNT> states
      GUARDED_BY(lock) = {};
};

GlobalThreadGlobals& GetGlobalThreadGlobals() {
  static base::NoDestructor<GlobalThreadGlobals> globals;
  return *globals;
}

std::string GetCurrentlyOnErrorMessage(GlobalThread::ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty()) {
    actual_name = "Unknown Thread";
  }
  std::string result = "Must be called on ";
  result += GlobalThread::GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

}  // namespace

namespace internal {
bool GlobalThreadChecker::CalledOnValidGlobalThread(
    GlobalThread::ID thread_identifier) const {
  return GlobalThread::CurrentlyOn(thread_identifier);
}

const GlobalThreadChecker& GetGlobalThreadChecker(
    GlobalThread::ID thread_identifier) {
  static std::array<GlobalThreadChecker, GlobalThread::ID_COUNT>
      global_thread_checkers;
  return global_thread_checkers[thread_identifier];
}
ScopedValidateGlobalThreadChecker::ScopedValidateGlobalThreadChecker(
    GlobalThread::ID thread_identifier,
    base::NotFatalUntil fatal_milestone) {
  const auto& checker = GetGlobalThreadChecker(thread_identifier);
  CHECK(checker.CalledOnValidGlobalThread(thread_identifier), fatal_milestone)
      << GetCurrentlyOnErrorMessage(thread_identifier);
}

ScopedValidateGlobalThreadChecker::~ScopedValidateGlobalThreadChecker() =
    default;

#if DCHECK_IS_ON()
ScopedValidateGlobalThreadDebugChecker::ScopedValidateGlobalThreadDebugChecker(
    GlobalThread::ID thread_identifier) {
  const auto& checker = GetGlobalThreadChecker(thread_identifier);
  DCHECK(checker.CalledOnValidGlobalThread(thread_identifier))
      << GetCurrentlyOnErrorMessage(thread_identifier);
}
#endif  // DCHECK_IS_ON()
}  // namespace internal

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

  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.states[ID::UI], State::UNINITIALIZED);
  DCHECK_EQ(globals.states[ID::IO], State::UNINITIALIZED);

  DCHECK(!globals.task_runners[ID::UI]);
  DCHECK(!globals.task_runners[ID::IO]);

  globals.task_runners[ID::UI] = main_thread_task_executor_->task_runner();
  globals.task_runners[ID::IO] = io_thread_->task_runner();
  globals.states[ID::UI] = State::RUNNING;
  globals.states[ID::IO] = State::RUNNING;
}

GlobalThread::~GlobalThread() {
  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);
  base::AutoLock lock(globals.lock);

  DCHECK(globals.states[ID::UI] == State::RUNNING ||
         globals.states[ID::UI] == State::SHUTDOWN);
  globals.states[ID::UI] = State::SHUTDOWN;
  globals.task_runners[ID::UI] = nullptr;

  DCHECK_EQ(globals.states[ID::IO], State::RUNNING);
  globals.states[ID::IO] = State::SHUTDOWN;
  globals.task_runners[ID::IO] = nullptr;
}

// Callable on any thread.  Returns whether you're currently on a particular
// thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
bool GlobalThread::CurrentlyOn(ID identifier) {
  auto task_runner = GetTaskRunnerForThread(identifier);
  return task_runner && task_runner->RunsTasksInCurrentSequence();
}

// static
bool GlobalThread::IsThreadInitialized(ID identifier) {
  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  base::AutoLock lock(globals.lock);
  return globals.states[identifier] == State::RUNNING;
}

// static
const char* GlobalThread::GetThreadName(ID identifier) {
  switch (identifier) {
    case UI:
      return "UI";
    case IO:
      return "IO";
    default:
      return "Unknown";
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
GlobalThread::GetUIThreadTaskRunner() {
  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  base::AutoLock lock(globals.lock);
  return globals.task_runners[ID::UI];
}

scoped_refptr<base::SingleThreadTaskRunner>
GlobalThread::GetIOThreadTaskRunner() {
  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  base::AutoLock lock(globals.lock);
  return globals.task_runners[ID::IO];
}

// static
bool GlobalThread::TryPostTaskToUIThread(const base::Location& from_here,
                                         base::OnceClosure task) {
  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  base::AutoLock lock(globals.lock);
  if (globals.states[ID::UI] != State::RUNNING ||
      !globals.task_runners[ID::UI]) {
    return false;
  }
  return globals.task_runners[ID::UI]->PostTask(from_here, std::move(task));
}

// static
void GlobalThread::BeginUIThreadShutdown() {
  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);
  base::AutoLock lock(globals.lock);
  DCHECK_NE(globals.states[ID::UI], State::UNINITIALIZED);
  globals.states[ID::UI] = State::SHUTDOWN;
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

}  // namespace lynxtron
