// Copyright (c) 2022 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <atlbase.h>  // ensures that ATL statics like `_AtlWinModule` are initialized (it's an issue in static debug build)
#include <tchar.h>
#include <windows.h>  // windows.h must be included first

#include <shellapi.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

// workaround for base/strings/strcat.h(18,9): error: 'StrCat' macro redefined
// [-Werror,-Wmacro-redefined]
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"

#include "base/at_exit.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/cstring_view.h"
#include "shell/app/library_main.h"
#include "shell/common/lynxtron_command_line.h"
// #include "base/strings/utf_string_conversions.h"
// #include "base/win/windows_version.h"
// #include "shell/app/library_main.h"
// #include "shell/common/lynxtron_command_line.h"
// #include "shell/app/command_line_args.h"
#pragma clang diagnostic pop

// #if !defined(ELECTRON_MAIN_EXECUTABLE)
// #include "shell/common/logging.h"
// #endif

// TODO(Guo Xi) : temporarily not support run as node
namespace {
// typedef int (*LynxtronMain_t)(int argc, wchar_t* argv_a[]);

// Redefined here so we don't have to introduce a dependency on //content
// from //electron:electron_app
// const char kUserDataDir[] = "user-data-dir";
// const char kProcessType[] = "type";

// [[nodiscard]] bool IsEnvSet(const base::cstring_view name) {
//   size_t required_size = 0;
//   getenv_s(&required_size, nullptr, 0, name.c_str());
//   return required_size != 0;
// }

}  // namespace

// In 32-bit builds, the main thread starts with the default (small) stack size.
// The ARCH_CPU_32_BITS blocks here and below are in support of moving the main
// thread to a fiber with a larger stack size.
#if defined(ARCH_CPU_32_BITS)
// The information needed to transfer control to the large-stack fiber and later
// pass the main routine's exit code back to the small-stack fiber prior to
// termination.
struct FiberState {
  HINSTANCE instance;
  LPVOID original_fiber;
  int fiber_result;
};

// A PFIBER_START_ROUTINE function run on a large-stack fiber that calls the
// main routine, stores its return value, and returns control to the small-stack
// fiber. |params| must be a pointer to a FiberState struct.
void WINAPI FiberBinder(void* params) {
  auto* fiber_state = static_cast<FiberState*>(params);
  // Call the wWinMain routine from the fiber. Reusing the entry point minimizes
  // confusion when examining call stacks in crash reports - seeing wWinMain on
  // the stack is a handy hint that this is the main thread of the process.
  fiber_state->fiber_result =
      wWinMain(fiber_state->instance, nullptr, nullptr, 0);
  // Switch back to the main thread to exit.
  ::SwitchToFiber(fiber_state->original_fiber);
}
#endif  // defined(ARCH_CPU_32_BITS)

bool ModuleCanBeRead(const base::FilePath& file_path) {
  return base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ)
      .IsValid();
}

base::FilePath GetModulePath(base::wcstring_view module_name) {
  base::FilePath exe_dir;
  const bool has_path = base::PathService::Get(base::DIR_EXE, &exe_dir);
  DCHECK(has_path);
  const base::FilePath module_path = exe_dir.Append(module_name);
  if (ModuleCanBeRead(module_path)) {
    return module_path;
  }

  // Othwerwise, return the path to the module in the current executable's
  // directory. This is the expected location of modules for dev builds.
  return exe_dir.Append(module_name);
}

// Prefetches and loads |module| after setting the CWD to |module|'s
// directory. Returns a handle to the loaded module on success, or nullptr on
// failure.
HMODULE LoadModuleWithDirectory(const base::FilePath& module) {
  //::SetCurrentDirectoryW(module.DirName().value().c_str());
  PLOG(ERROR) << module.value().c_str();
  base::PreReadFile(module, /*is_executable=*/true, false);
  HMODULE handle = ::LoadLibraryExW(module.value().c_str(), nullptr,
                                    LOAD_WITH_ALTERED_SEARCH_PATH);
  return handle;
}

// Prefetches and loads the appropriate DLL for the process type
// |process_type_|. Populates |module| with the path of the loaded DLL.
// Returns a handle to the loaded DLL, or nullptr on failure.
HMODULE Load(const base::FilePath& module) {
  if (module.empty()) {
    PLOG(ERROR) << "Cannot find module lxtn.dll";
    return nullptr;
  }
  HMODULE dll = LoadModuleWithDirectory(module);
  if (!dll) {
    PLOG(ERROR) << "Failed to load Lynxtron DLL from " << module.value();
  }
  return dll;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* cmd, int) {
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
    PLOG(ERROR) << "Failed to get executable directory";
    return 1;
  }
  base::FilePath file = exe_dir.Append(L"lxtn.dll");
  HMODULE dll = Load(file);
  LynxtronMainPtr entry_point = reinterpret_cast<LynxtronMainPtr>(
      reinterpret_cast<void*>(::GetProcAddress(dll, "LynxtronMain")));
  int exit_code = entry_point();
  return exit_code;
}
