// Copyright (c) 2022 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <windows.h>  // windows.h must be included first

#include "base/at_exit.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/scoped_native_library.h"
#include "shell/app/library_main.h"

#if defined(ARCH_CPU_32_BITS)
struct FiberState {
  HINSTANCE instance;
  LPVOID original_fiber;
  int fiber_result;
};

void WINAPI FiberBinder(void* params) {
  auto* fiber_state = static_cast<FiberState*>(params);
  fiber_state->fiber_result =
      wWinMain(fiber_state->instance, nullptr, nullptr, 0);
  ::SwitchToFiber(fiber_state->original_fiber);
}
#endif

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
#if defined(ARCH_CPU_32_BITS)
  enum class FiberStatus { kConvertFailed, kCreateFiberFailed, kSuccess };
  FiberStatus fiber_status = FiberStatus::kSuccess;
  // GetLastError result if fiber conversion failed.
  DWORD fiber_error = ERROR_SUCCESS;
  if (!::IsThreadAFiber()) {
    // Make the main thread's stack size 4 MiB so that it has roughly the same
    // effective size as the 64-bit build's 8 MiB stack.
    constexpr size_t kStackSize = 4 * 1024 * 1024;  // 4 MiB
    // Leak the fiber on exit.
    LPVOID original_fiber =
        ::ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
    if (original_fiber) {
      FiberState fiber_state = {instance, original_fiber};
      // Create a fiber with a bigger stack and switch to it. Leak the fiber on
      // exit.
      LPVOID big_stack_fiber = ::CreateFiberEx(
          0, kStackSize, FIBER_FLAG_FLOAT_SWITCH, FiberBinder, &fiber_state);
      if (big_stack_fiber) {
        ::SwitchToFiber(big_stack_fiber);
        // The fibers must be cleaned up to avoid obscure TLS-related shutdown
        // crashes.
        ::DeleteFiber(big_stack_fiber);
        ::ConvertFiberToThread();
        // Control returns here after Chrome has finished running on FiberMain.
        return fiber_state.fiber_result;
      }
      fiber_status = FiberStatus::kCreateFiberFailed;
    } else {
      fiber_status = FiberStatus::kConvertFailed;
    }
    // If we reach here then creating and switching to a fiber has failed. This
    // probably means we are low on memory and will soon crash. Try to report
    // this error once crash reporting is initialized.
    fiber_error = ::GetLastError();
    base::debug::Alias(&fiber_error);
  }
  // If we are already a fiber then continue normal execution.
#endif  // defined(ARCH_CPU_32_BITS)

#if defined(ARCH_CPU_32_BITS)
  // Intentionally crash if converting to a fiber failed.
  CHECK_EQ(fiber_status, FiberStatus::kSuccess);
#endif  // defined(ARCH_CPU_32_BITS)

  base::AtExitManager atexit_manager;
  base::RouteStdioToConsole(false);
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get executable directory";
    return 1;
  }

  base::FilePath library_path =
      exe_dir.AppendASCII(base::GetNativeLibraryName("lxtn"));
  if (!base::PathExists(library_path)) {
    LOG(ERROR) << "Library does not exist: " << library_path.value();
    return 1;
  }
  base::PreReadFile(library_path, /*is_executable=*/true, /*sequential=*/false);
  base::ScopedNativeLibrary library(library_path);
  if (!library.is_valid()) {
    const base::NativeLibraryLoadError* load_error = library.GetError();
    if (load_error) {
      LOG(ERROR) << "Failed to load library: " << library_path.value()
                 << ", error_code=" << load_error->ToString();
    } else {
      LOG(ERROR) << "Failed to load library: " << library_path.value();
    }
    return 1;
  }
  LynxtronMainPtr entry_point = reinterpret_cast<LynxtronMainPtr>(
      library.GetFunctionPointer("LynxtronMain"));
  if (!entry_point) {
    LOG(ERROR) << "Failed to resolve LynxtronMain";
    return 1;
  }
  return entry_point();
}
