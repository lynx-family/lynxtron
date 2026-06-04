// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/common/global_thread.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_checker.h"
#include "shell/common/io_thread.h"

namespace lynxtron {
namespace {

// State of a given GlobalThread::ID in chronological order throughout the
// process' lifetime.
enum GlobalThreadState {
  // GlobalThread::ID isn't associated with anything yet.
  UNINITIALIZED = 0,
  // GlobalThread::ID is associated to a TaskRunner and is accepting tasks.
  RUNNING,
  // GlobalThread::ID no longer accepts tasks (it's still associated to a
  // TaskRunner but that TaskRunner doesn't have to accept tasks).
  SHUTDOWN
};

struct GlobalThreadGlobals {
  GlobalThreadGlobals() {
    // GlobalThreadGlobals must be initialized on main thread before it's used
    // by any other threads.
    DETACH_FROM_THREAD(main_thread_checker_);
  }

  THREAD_CHECKER(main_thread_checker_);

  // |task_runners[id]| is safe to access on |main_thread_checker_| as
  // well as on any thread once it's read-only after initialization
  // (i.e. while |states[id] >= RUNNING|).
  std::array<scoped_refptr<base::SingleThreadTaskRunner>,
             GlobalThread::ID_COUNT>
      task_runners;

  // Tracks the runtime state of GlobalThreads. Atomic because a few
  // methods below read this value outside |main_thread_checker_| to
  // confirm it's >= RUNNING and doing so requires an atomic read as it could be
  // in the middle of transitioning to SHUTDOWN (which check is fine with
  // but reading a non-atomic value as it's written to by another thread can be
  // undefined behavior on some platforms).
  std::array<std::atomic<GlobalThreadState>, GlobalThread::ID_COUNT> states =
      {};
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
  DCHECK_EQ(globals.states[ID::UI].load(std::memory_order_relaxed),
            GlobalThreadState::UNINITIALIZED);
  globals.states[ID::UI].store(GlobalThreadState::RUNNING,
                               std::memory_order_relaxed);

  DCHECK_EQ(globals.states[ID::IO].load(std::memory_order_relaxed),
            GlobalThreadState::UNINITIALIZED);
  globals.states[ID::IO].store(GlobalThreadState::RUNNING,
                               std::memory_order_relaxed);

  DCHECK(!globals.task_runners[ID::UI]);
  DCHECK(!globals.task_runners[ID::IO]);

  globals.task_runners[ID::UI] = main_thread_task_executor_->task_runner();
  globals.task_runners[ID::IO] = io_thread_->task_runner();
}

GlobalThread::~GlobalThread() {
  GlobalThreadGlobals& globals = GetGlobalThreadGlobals();
  DCHECK_CALLED_ON_VALID_THREAD(globals.main_thread_checker_);

  DCHECK_EQ(globals.states[ID::UI].load(std::memory_order_relaxed),
            GlobalThreadState::RUNNING);
  globals.states[ID::UI].store(GlobalThreadState::SHUTDOWN,
                               std::memory_order_relaxed);
  globals.task_runners[ID::UI] = nullptr;

  DCHECK_EQ(globals.states[ID::IO].load(std::memory_order_relaxed),
            GlobalThreadState::RUNNING);
  globals.states[ID::IO].store(GlobalThreadState::SHUTDOWN,
                               std::memory_order_relaxed);
  globals.task_runners[ID::IO] = nullptr;
}

// Callable on any thread.  Returns whether you're currently on a particular
// thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
bool GlobalThread::CurrentlyOn(ID identifier) {
  return GetGlobalThreadGlobals()
      .task_runners[identifier]
      ->RunsTasksInCurrentSequence();
}

// static
bool GlobalThread::IsThreadInitialized(ID identifier) {
  return GetGlobalThreadGlobals().states[identifier].load(
             std::memory_order_relaxed) == GlobalThreadState::RUNNING;
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
  return GetGlobalThreadGlobals().task_runners[ID::UI];
}

scoped_refptr<base::SingleThreadTaskRunner>
GlobalThread::GetIOThreadTaskRunner() {
  return GetGlobalThreadGlobals().task_runners[ID::IO];
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
