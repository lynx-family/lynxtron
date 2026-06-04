// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_COMMON_GLOBAL_THREAD_H_
#define LYNXTRON_SHELL_COMMON_GLOBAL_THREAD_H_

#include <memory>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_executor.h"
#include "base/thread_annotations.h"
#include "shell/common/io_thread.h"

namespace lynxtron {

// Use DCHECK_CURRENTLY_ON(GlobalThread::ID) to DCHECK that a function can only
// be called on the named GlobalThread.
#define DCHECK_CURRENTLY_ON(thread_identifier)                                \
  ::lynxtron::internal::ScopedValidateGlobalThreadDebugChecker BASE_UNIQUIFY( \
      scoped_validate_global_thread_dchecker_)(thread_identifier)

// Use CHECK_CURRENTLY_ON(GlobalThread::ID) to CHECK that a function can only
// be called on the named GlobalThread.
#define CHECK_CURRENTLY_ON(thread_identifier, ...)                             \
  ::lynxtron::internal::ScopedValidateGlobalThreadChecker BASE_UNIQUIFY(       \
      scoped_validate_global_thread_checker_)(thread_identifier __VA_OPT__(, ) \
                                                  __VA_ARGS__)

// GUARDED_BY_GLOBAL_THREAD() enforces that a member variable is only accessed
// from a scope that invokes DCHECK_CURRENTLY_ON() or CHECK_CURRENTLY_ON() or
// from a function annotated with VALID_GLOBAL_THREAD_REQUIRED(). The code will
// not compile if the member variable is accessed and these conditions are not
// met.
#define GUARDED_BY_GLOBAL_THREAD(thread_identifier) \
  GUARDED_BY(::lynxtron::internal::GetGlobalThreadChecker(thread_identifier))

// VALID_GLOBAL_THREAD_REQUIRED() enforces that a member function is only
// accessed from a scope that invokes DCHECK_CURRENTLY_ON() or
// CHECK_CURRENTLY_ON() or from another function annotated with
// VALID_GLOBAL_THREAD_REQUIRED(). The code will not compile if the member
// function is accessed and these conditions are not met.
#define VALID_GLOBAL_THREAD_REQUIRED(thread_identifier) \
  EXCLUSIVE_LOCKS_REQUIRED(                             \
      ::lynxtron::internal::GetGlobalThreadChecker(thread_identifier))

// The main entry point to post tasks to the main (UI) thread.
// GlobalThread must be created before this API is used.
scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner();

// The GlobalThread::IO counterpart to GetUIThreadTaskRunner().
scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner();

class GlobalThread {
 public:
  // An enumeration of the well-known threads.
  enum ID {
    // The main thread. It stops running tasks during shutdown and is never
    // joined.
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

  // State of a given GlobalThread::ID in chronological order throughout the
  // process' lifetime.
  enum State {
    // GlobalThread::ID isn't associated with anything yet.
    UNINITIALIZED = 0,
    // GlobalThread::ID is associated to a TaskRunner and is accepting tasks.
    RUNNING,
    // GlobalThread::ID no longer accepts tasks (it's still associated to a
    // TaskRunner but that TaskRunner doesn't have to accept tasks).
    SHUTDOWN
  };

  GlobalThread();
  ~GlobalThread();

  GlobalThread(const GlobalThread&) = delete;
  GlobalThread& operator=(const GlobalThread&) = delete;

  // Callable on any thread.  Returns whether you're currently on a particular
  // thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
  [[nodiscard]] static bool CurrentlyOn(ID identifier);

  [[nodiscard]] static bool IsThreadInitialized(ID identifier);

  [[nodiscard]] static const char* GetThreadName(ID identifier);

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
  // SequencedTaskRunner instead of specific GlobalThreads.
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
  //           Foo, GlobalThread::DeleteOnIOThread> {
  //
  // ...
  //  private:
  //   friend struct GlobalThread::DeleteOnThread<GlobalThread::IO>;
  //   friend class base::DeleteHelper<Foo>;
  //
  //   ~Foo();
  //
  // Sample usage with scoped_ptr:
  // std::unique_ptr<Foo, GlobalThread::DeleteOnIOThread> ptr;
  //
  // Note: see base::OnTaskRunnerDeleter and base::RefCountedDeleteOnSequence to
  // bind to SequencedTaskRunner instead of specific GlobalThreads.
  struct DeleteOnUIThread : public DeleteOnThread<UI> {};
  struct DeleteOnIOThread : public DeleteOnThread<IO> {};

  // Helper that returns GetUIThreadTaskRunner() or GetIOThreadTaskRunner()
  // based on |identifier|. Requires that the GlobalThread with the provided
  // |identifier| was started.
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForThread(
      ID identifier);

 private:
  std::unique_ptr<base::SingleThreadTaskExecutor> main_thread_task_executor_;
  std::unique_ptr<IOThread> io_thread_;
};

namespace internal {

class THREAD_ANNOTATION_ATTRIBUTE__(capability("GlobalThread checker"))
    GlobalThreadChecker {
 public:
  [[nodiscard]] bool CalledOnValidGlobalThread(
      GlobalThread::ID thread_identifier) const;
};

// Returns the global GlobalThreadChecker associated with `thread_identifier`.
const GlobalThreadChecker& GetGlobalThreadChecker(
    GlobalThread::ID thread_identifier);

// CHECK version.
class SCOPED_LOCKABLE ScopedValidateGlobalThreadChecker {
 public:
  explicit ScopedValidateGlobalThreadChecker(
      GlobalThread::ID thread_identifier,
      base::NotFatalUntil fatal_milestone =
          base::NotFatalUntil::NoSpecifiedMilestoneInternal)
      EXCLUSIVE_LOCK_FUNCTION(GetGlobalThreadChecker(thread_identifier));
  ~ScopedValidateGlobalThreadChecker() UNLOCK_FUNCTION();
};

// DCHECK version.
// Note: When DCHECKs are disabled, this class needs to be completely optimized
// out in order to not regress binary size. This is achieved by inlining the
// constructor and the destructor. When DCHECKs are enabled, the constructor
// is not unnecessarily inlined.
class SCOPED_LOCKABLE ScopedValidateGlobalThreadDebugChecker {
 public:
  explicit ScopedValidateGlobalThreadDebugChecker(
      GlobalThread::ID thread_identifier)
      EXCLUSIVE_LOCK_FUNCTION(GetGlobalThreadChecker(thread_identifier))
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
  ~ScopedValidateGlobalThreadDebugChecker() UNLOCK_FUNCTION() {}
};
}  // namespace internal

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_COMMON_GLOBAL_THREAD_H_
