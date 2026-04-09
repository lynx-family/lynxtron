// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/io_thread.h"

#include "base/clang_profiling_buildflags.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace lynxtron {

IOThread::IOThread() : base::Thread("IOThread") {
  // Not bound to creation thread.
  DETACH_FROM_THREAD(thread_checker_);
}

IOThread::~IOThread() {
  Stop();
}

void IOThread::AllowBlockingForTesting() {
  DCHECK(!IsRunning());
  is_blocking_allowed_for_testing_ = true;
}

void IOThread::Init() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if BUILDFLAG(IS_WIN)
  com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
#endif

  if (!is_blocking_allowed_for_testing_) {
    base::DisallowUnresponsiveTasks();
  }
}

void IOThread::Run(base::RunLoop* run_loop) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  IOThreadRun(run_loop);
}

void IOThread::CleanUp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if BUILDFLAG(IS_WIN)
  com_initializer_.reset();
#endif
}

void IOThread::IOThreadRun(base::RunLoop* run_loop) {
  // Register the IO thread for hang watching before it starts running and set
  // up a closure to automatically unregister it when Run() returns.
  base::ScopedClosureRunner unregister_thread_closure;
  if (base::HangWatcher::IsIOThreadHangWatchingEnabled()) {
    unregister_thread_closure = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kIOThread);
  }

  Thread::Run(run_loop);
  NO_CODE_FOLDING();
}
}  // namespace lynxtron
