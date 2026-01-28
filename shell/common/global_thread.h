// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_COMMON_GLOBAL_THREAD_H_
#define LYNXTRON_SHELL_COMMON_GLOBAL_THREAD_H_

#include <memory>

#include "base/logging.h"
#include "base/task/single_thread_task_executor.h"
#include "base/thread_annotations.h"
#include "shell/common/io_thread.h"

// TODO(Guo Xi): organize the naming, and the naming with browser

namespace lynxtron {

// Use DCHECK_CURRENTLY_ON(BrowserThread::ID) to DCHECK that a function can only
// be called on the named BrowserThread.
#define DCHECK_CURRENTLY_ON(thread_identifier)                                 \
  ::lynxtron::internal::ScopedValidateBrowserThreadDebugChecker BASE_UNIQUIFY( \
      scoped_validate_browser_thread_dchecker_)(thread_identifier)

// Use CHECK_CURRENTLY_ON(BrowserThread::ID) to CHECK that a function can only
// be called on the named BrowserThread.
#define CHECK_CURRENTLY_ON(thread_identifier, ...)                        \
  ::lynxtron::internal::ScopedValidateBrowserThreadChecker BASE_UNIQUIFY( \
      scoped_validate_browser_thread_checker_)(                           \
      thread_identifier __VA_OPT__(, ) __VA_ARGS__)

// GUARDED_BY_BROWSER_THREAD() enforces that a member variable is only accessed
// from a scope that invokes DCHECK_CURRENTLY_ON() or CHECK_CURRENTLY_ON() or
// from a function annotated with VALID_BROWSER_THREAD_REQUIRED(). The code will
// not compile if the member variable is accessed and these conditions are not
// met.
#define GUARDED_BY_BROWSER_THREAD(thread_identifier) \
  GUARDED_BY(::lynxtron::internal::GetBrowserThreadChecker(thread_identifier))

// VALID_CONTEXT_REQUIRED() enforces that a member function is only accessed
// from a scope that invokes DCHECK_CURRENTLY_ON() or CHECK_CURRENTLY_ON() or
// from another function annotated with VALID_BROWSER_THREAD_REQUIRED(). The
// code will not compile if the member function is accessed and these conditions
// are not met.
#define VALID_BROWSER_THREAD_REQUIRED(thread_identifier) \
  EXCLUSIVE_LOCKS_REQUIRED(                              \
      ::lynxtron::internal::GetBrowserThreadChecker(thread_identifier))

// The main entry point to post tasks to the UI thread. Tasks posted with the
// same |traits| will run in posting order (i.e. according to the
// SequencedTaskRunner contract). Tasks posted with different |traits| can be
// re-ordered. You may keep a reference to this task runner, it's always
// thread-safe to post to it though it may start returning false at some point
// during shutdown when it definitely is no longer accepting tasks.
//
// In unit tests, there must be a content::BrowserTaskEnvironment in scope for
// this API to be available.
scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner();

// The BrowserThread::IO counterpart to GetUIThreadTaskRunner().
scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner();

class GlobalThread {
 public:
  // An enumeration of the well-known threads.
  enum ID {
    // The main thread in the browser. It stops running tasks during shutdown
    // and is never joined.
    UI,

    // This is the thread that processes non-blocking I/O, i.e. IPC and network.
    // Blocking I/O should happen in base::ThreadPool. It is joined on shutdown
    // (and thus any task posted to it may block shutdown).
    //
    // The name is admittedly confusing, as the IO thread is not for blocking
    // I/O like calling base::File::Read. "The highly responsive, non-blocking
    // I/O thread for IPC" is more accurate but too long for an enum name. See
    // docs/transcripts/wuwt-e08-processes.md at 44:20 for more history.
    IO,

    // NOTE: do not add new threads here. Instead you should just use
    // base::ThreadPool::Create*TaskRunner to run tasks on the base::ThreadPool.

    // This identifier does not represent a thread.  Instead it counts the
    // number of well-known threads.  Insert new well-known threads before this
    // identifier.
    ID_COUNT
  };

  // State of a given BrowserThread::ID in chronological order throughout the
  // browser process' lifetime.
  enum BrowserThreadState {
    // BrowserThread::ID isn't associated with anything yet.
    UNINITIALIZED = 0,
    // BrowserThread::ID is associated to a TaskRunner and is accepting tasks.
    RUNNING,
    // BrowserThread::ID no longer accepts tasks (it's still associated to a
    // TaskRunner but that TaskRunner doesn't have to accept tasks).
    SHUTDOWN
  };

  static void Create();

  GlobalThread(const GlobalThread&) = delete;
  GlobalThread& operator=(const GlobalThread&) = delete;

  // Callable on any thread.  Returns whether you're currently on a particular
  // thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
  [[nodiscard]] static bool CurrentlyOn(ID identifier);

  [[nodiscard]] static bool IsThreadInitialized(ID identifier);

  static scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner();

  static scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner();

  // Use these templates in conjunction with RefCountedThreadSafe or scoped_ptr
  // when you want to ensure that an object is deleted on a specific thread.
  // This is needed when an object can hop between threads (i.e. UI -> IO ->
  // UI), and thread switching delays can mean that the final UI tasks executes
  // before the IO task's stack unwinds. This would lead to the object
  // destructing on the IO thread, which often is not what you want (i.e. to
  // notify other objects on the creating thread etc). Note: see
  // base::OnTaskRunnerDeleter and base::RefCountedDeleteOnSequence to bind to
  // SequencedTaskRunner instead of specific BrowserThreads.
  template <ID thread>
  struct DeleteOnThread {
    template <typename T>
    static void Destruct(const T* x) {
      if (CurrentlyOn(thread)) {
        delete x;
      } else {
        if (!GetTaskRunnerForThread(thread)->DeleteSoon(FROM_HERE, x)) {
#if defined(UNIT_TEST)
          // Only logged under unit testing because leaks at shutdown
          // are acceptable under normal circumstances.
          LOG(ERROR) << "DeleteSoon failed on thread " << thread;
#endif  // UNIT_TEST
        }
      }
    }
    template <typename T>
    inline void operator()(T* ptr) const {
      enum { type_must_be_complete = sizeof(T) };
      Destruct(ptr);
    }
  };

  // Sample usage with RefCountedThreadSafe:
  // class Foo
  //     : public base::RefCountedThreadSafe<
  //           Foo, BrowserThread::DeleteOnIOThread> {
  //
  // ...
  //  private:
  //   friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  //   friend class base::DeleteHelper<Foo>;
  //
  //   ~Foo();
  //
  // Sample usage with scoped_ptr:
  // std::unique_ptr<Foo, BrowserThread::DeleteOnIOThread> ptr;
  //
  // Note: see base::OnTaskRunnerDeleter and base::RefCountedDeleteOnSequence to
  // bind to SequencedTaskRunner instead of specific BrowserThreads.
  struct DeleteOnUIThread : public DeleteOnThread<UI> {};
  struct DeleteOnIOThread : public DeleteOnThread<IO> {};

  // Helper that returns GetUIThreadTaskRunner({}) or GetIOThreadTaskRunner({})
  // based on |identifier|. Requires that the BrowserThread with the provided
  // |identifier| was started.
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForThread(
      ID identifier);

 private:
  static GlobalThread* Get();
  GlobalThread();
  ~GlobalThread();

  std::unique_ptr<base::SingleThreadTaskExecutor> main_thread_task_executor_;
  std::unique_ptr<IOThread> io_thread_;
  std::array<scoped_refptr<base::SingleThreadTaskRunner>,
             GlobalThread::ID_COUNT>
      task_runners_;
  std::atomic<BrowserThreadState> states_[GlobalThread::ID_COUNT] = {};
};

namespace internal {

class THREAD_ANNOTATION_ATTRIBUTE__(capability("BrowserThread checker"))
    BrowserThreadChecker {
 public:
  [[nodiscard]] bool CalledOnValidBrowserThread(
      GlobalThread::ID thread_identifier) const;
};

// Returns the global BrowserThreadChecker associated with `thread_identifier`.
const BrowserThreadChecker& GetBrowserThreadChecker(
    GlobalThread::ID thread_identifier);

// CHECK version.
class SCOPED_LOCKABLE ScopedValidateBrowserThreadChecker {
 public:
  explicit ScopedValidateBrowserThreadChecker(
      GlobalThread::ID thread_identifier,
      base::NotFatalUntil fatal_milestone =
          base::NotFatalUntil::NoSpecifiedMilestoneInternal)
      EXCLUSIVE_LOCK_FUNCTION(GetBrowserThreadChecker(thread_identifier));
  ~ScopedValidateBrowserThreadChecker() UNLOCK_FUNCTION();
};

// DCHECK version.
// Note: When DCHECKs are disabled, this class needs to be completely optimized
// out in order to not regress binary size. This is achieved by inlining the
// constructor and the destructor. When DCHECKs are enabled, the constructor
// is not unnecessarily inlined.
class SCOPED_LOCKABLE ScopedValidateBrowserThreadDebugChecker {
 public:
  explicit ScopedValidateBrowserThreadDebugChecker(
      GlobalThread::ID thread_identifier)
      EXCLUSIVE_LOCK_FUNCTION(GetBrowserThreadChecker(thread_identifier))
// Only inlined when DCHECKs are turned off.
#if DCHECK_IS_ON()
          ;
#else
  {
  }
#endif

  // Note: Can't use = default as it does not work well with UNLOCK_FUNCTION().
  // Clang will discard the UNLOCK_FUNCTION() attribute.
  // See https://github.com/llvm/llvm-project/issues/101199.
  ~ScopedValidateBrowserThreadDebugChecker() UNLOCK_FUNCTION() {}
};
}  // namespace internal

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_GLOBAL_THREAD_H_
