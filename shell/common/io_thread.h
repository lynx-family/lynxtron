// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_COMMON_IO_THREAD_H_
#define LYNXTRON_SHELL_COMMON_IO_THREAD_H_

#include <memory>

#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
namespace base {
namespace win {
class ScopedCOMInitializer;
}
}  // namespace base
#endif

namespace lynxtron {

// ----------------------------------------------------------------------------
// A IOThread is a physical thread backing the IO thread.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class IOThread : public base::Thread {
 public:
  // Constructs a IOThread.
  IOThread();

  IOThread(const IOThread&) = delete;
  IOThread& operator=(const IOThread&) = delete;

  ~IOThread() override;

  // TODO(Guo Xi): remove this function, it may not be needed
  // Ideally there wouldn't be a special blanket allowance to block the
  // BrowserThreads in tests but TestBrowserThreadImpl previously bypassed
  // IOThread and hence wasn't subject to ThreadRestrictions...
  // Flipping that around in favor of explicit scoped allowances would be
  // preferable but a non-trivial amount of work. Can only be called before
  // starting this IOThread.
  void AllowBlockingForTesting();

 protected:
  void Init() override;
  void Run(base::RunLoop* run_loop) override;
  void CleanUp() override;

 private:
  void IOThreadRun(base::RunLoop* run_loop);

  // BrowserThreads are not allowed to do file I/O nor wait on synchronization
  // primivives except when explicitly allowed in tests.
  bool is_blocking_allowed_for_testing_ = false;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  THREAD_CHECKER(thread_checker_);
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_IO_THREAD_H_
