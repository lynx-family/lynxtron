// Copyright (c) 2021 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/logging.h"

#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_number_conversions.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/path_provider.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#include "base/win/windows_handle_util.h"
#endif

namespace lynxtron {

// Overrides the default file name to use for general-purpose logging (does not
// affect which events are logged).
const char kLogFile[] = "log-file";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[] = "enable-logging";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[] = "log-level";

constexpr base::cstring_view kLogFileName{"LYNXTRON_LOG_FILE"};
constexpr base::cstring_view kLynxtronEnableLogging{"LYNXTRON_ENABLE_LOGGING"};

#if BUILDFLAG(IS_WIN)
base::win::ScopedHandle GetLogInheritedHandle(
    const base::CommandLine& command_line) {
  auto handle_str = command_line.GetSwitchValueNative(kLogFile);
  uint32_t handle_value = 0;
  if (!base::StringToUint(handle_str, &handle_value)) {
    return {};
  }
  // Duplicate the handle from the command line so that different things can
  // init logging. This means the handle from the parent is never closed, but
  // there will only be one of these in the process.
  HANDLE log_handle = nullptr;
  if (!::DuplicateHandle(::GetCurrentProcess(),
                         base::win::Uint32ToHandle(handle_value),
                         ::GetCurrentProcess(), &log_handle, 0,
                         /*bInheritHandle=*/FALSE, DUPLICATE_SAME_ACCESS)) {
    return {};
  }
  // Transfer ownership to the caller.
  return base::win::ScopedHandle(log_handle);
}
#endif

namespace {

std::string GetExplicitLogFileValue(const base::CommandLine& command_line,
                                    base::Environment& env) {
  std::string filename = command_line.GetSwitchValueASCII(kLogFile);
  if (!filename.empty()) {
    return filename;
  }

  return env.GetVar(kLogFileName).value_or("");
}

base::FilePath GetLogFileNameInternal(const base::CommandLine& command_line,
                                      base::Environment& env) {
  std::string filename = GetExplicitLogFileValue(command_line, env);
  if (!filename.empty()) {
    return base::FilePath::FromUTF8Unsafe(filename);
  }

  auto log_filename = base::FilePath{FILE_PATH_LITERAL("lynxtron_debug.log")};

  if (base::FilePath path;
      base::PathService::Get(lynxtron::DIR_APP_LOGS, &path)) {
    return path.Append(log_filename);
  }

  // error with path service, just use some default file somewhere
  return log_filename;
}

bool HasExplicitLogFileInternal(const base::CommandLine& command_line,
                                base::Environment& env) {
  return !GetExplicitLogFileValue(command_line, env).empty();
}

std::pair<::logging::LoggingDestination, bool /* filename_is_handle */>
DetermineLoggingDestination(const base::CommandLine& command_line,
                            bool is_preinit,
                            base::Environment& env) {
  bool enable_logging = false;
  std::string logging_destination;
  if (command_line.HasSwitch(kEnableLogging)) {
    enable_logging = true;
    logging_destination = command_line.GetSwitchValueASCII(kEnableLogging);
  } else if (env.HasVar(kLynxtronEnableLogging)) {
    enable_logging = true;
    logging_destination = env.GetVar(kLynxtronEnableLogging).value();
  }
  if (!enable_logging) {
    return {::logging::LOG_NONE, false};
  }

  bool also_log_to_stderr = false;
#if !defined(NDEBUG)
  if (std::optional<std::string> also_log_to_stderr_str =
          env.GetVar("LYNXTRON_ALSO_LOG_TO_STDERR")) {
    also_log_to_stderr = !also_log_to_stderr_str->empty();
  }
#endif

#if BUILDFLAG(IS_WIN)
  if (logging_destination == "handle" && command_line.HasSwitch(kLogFile)) {
    // Child processes can log to a handle duplicated from the parent, and
    // provided in the log-file switch value.
    return {::logging::LOG_TO_FILE, true};
  }
#endif  // BUILDFLAG(IS_WIN)

  // Follow Chromium behavior: --enable-logging logs to a file, and
  // --enable-logging=stderr logs to stderr.
  // If --log-file or LYNXTRON_LOG_FILE is specified along with
  // --enable-logging, log to that file. During pre-init, avoid logging to the
  // default log file (inside the user data directory) until it's known. Log to
  // stderr unless an explicit filename is given.
  if (HasExplicitLogFileInternal(command_line, env)) {
    return {::logging::LOG_TO_FILE |
                (also_log_to_stderr ? ::logging::LOG_TO_STDERR : 0),
            false};
  }

  if (logging_destination == "stderr" || is_preinit) {
    return {::logging::LOG_TO_SYSTEM_DEBUG_LOG | ::logging::LOG_TO_STDERR,
            false};
  }

  return {::logging::LOG_TO_FILE |
              (also_log_to_stderr ? ::logging::LOG_TO_STDERR : 0),
          false};
}

}  // namespace

base::FilePath GetLogFileName(const base::CommandLine& command_line) {
  auto env = base::Environment::Create();
  return GetLogFileNameInternal(command_line, *env);
}

void InitLogging(const base::CommandLine& command_line, bool is_preinit) {
  auto env = base::Environment::Create();
  auto [logging_dest, filename_is_handle] =
      DetermineLoggingDestination(command_line, is_preinit, *env);
  ::logging::LogLockingState log_locking_state = ::logging::LOCK_LOG_FILE;
  base::FilePath log_path;
#if BUILDFLAG(IS_WIN)
  base::win::ScopedHandle log_handle;
#endif

  if (command_line.HasSwitch(kLoggingLevel) &&
      ::logging::GetMinLogLevel() >= 0) {
    std::string log_level = command_line.GetSwitchValueASCII(kLoggingLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level) && level >= 0 &&
        level < ::logging::LOGGING_NUM_SEVERITIES) {
      ::logging::SetMinLogLevel(level);
    } else {
      DLOG(WARNING) << "Bad log level: " << log_level;
    }
  }

  // Don't resolve the log path unless we need to. Otherwise we leave an open
  // ALPC handle after sandbox lockdown on Windows.
  if ((logging_dest & ::logging::LOG_TO_FILE) != 0) {
    if (filename_is_handle) {
#if BUILDFLAG(IS_WIN)
      // Child processes on Windows are provided a file handle if logging is
      // enabled as sandboxed processes cannot open files.
      log_handle = GetLogInheritedHandle(command_line);
      if (!log_handle.is_valid()) {
        LOG(ERROR) << "Unable to initialize logging from handle.";
        return;
      }
#endif
    } else {
      log_path = GetLogFileNameInternal(command_line, *env);
    }
  } else {
    log_locking_state = ::logging::DONT_LOCK_LOG_FILE;
  }

  // On Windows, having non canonical forward slashes in log file name causes
  // problems with sandbox filters, see https://crbug.com/859676
  log_path = log_path.NormalizePathSeparators();

  ::logging::LoggingSettings settings;
  settings.logging_dest = logging_dest;
  settings.log_file_path = log_path.value().c_str();
#if BUILDFLAG(IS_WIN)
  // Avoid initializing with INVALID_HANDLE_VALUE.
  // This handle is owned by the logging framework and is closed when the
  // process exits.
  // TODO(crbug.com/328285906) Use a ScopedHandle in logging settings.
  settings.log_file = log_handle.is_valid() ? log_handle.release() : nullptr;
#endif
  settings.lock_log = log_locking_state;
  // If we're logging to an explicit file passed with --log-file, we don't want
  // to delete the log file on our second initialization.
  settings.delete_old =
      (is_preinit || !HasExplicitLogFileInternal(command_line, *env))
          ? ::logging::DELETE_OLD_LOG_FILE
          : ::logging::APPEND_TO_OLD_LOG_FILE;
  bool success = ::logging::InitLogging(settings);
  if (!success) {
    PLOG(ERROR) << "Failed to init logging";
  }

  ::logging::SetLogItems(true /* pid */, false, true /* timestamp */, false);
}

}  // namespace lynxtron
