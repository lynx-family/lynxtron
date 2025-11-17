// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/app/application.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "shell/app/application_observer.h"
#include "shell/app/main_parts.h"
#include "shell/app/native_window.h"
#include "shell/app/window_list.h"
#include "shell/common/application_info.h"
#include "shell/common/lynxtron_paths.h"
#include "shell/common/thread_restrictions.h"

namespace lynxtron {

LoginItemSettings::LoginItemSettings() = default;
LoginItemSettings::~LoginItemSettings() = default;
LoginItemSettings::LoginItemSettings(const LoginItemSettings& other) = default;

#if BUILDFLAG(IS_WIN)
LaunchItem::LaunchItem() = default;
LaunchItem::~LaunchItem() = default;
LaunchItem::LaunchItem(const LaunchItem& other) = default;
#endif

namespace {

// Call |quit| after Chromium is fully started.
//
// This is important for quitting immediately in the "ready" event, when
// certain initialization task may still be pending, and quitting at that time
// could end up with crash on exit.
void RunQuitClosure(base::OnceClosure quit) {
  // On Linux/Windows the "ready" event is emitted in "PreMainMessageLoopRun",
  // make sure we quit after message loop has run for once.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(quit));
}

}  // namespace

Application::Application() {
  WindowList::AddObserver(this);
}

Application::~Application() {
  WindowList::RemoveObserver(this);
}

void Application::AddObserver(ApplicationObserver* obs) {
  observers_.AddObserver(obs);
}

void Application::RemoveObserver(ApplicationObserver* obs) {
  observers_.RemoveObserver(obs);
}

// static
Application* Application::Get() {
  return MainParts::Get()->application();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
void Application::Focus(gin::Arguments* args) {
  // Focus on the first visible window.
  for (auto* const window : WindowList::GetWindows()) {
    if (window->IsVisible()) {
      window->Focus(true);
      break;
    }
  }
}
#endif

void Application::Quit() {
  if (is_quitting_) {
    return;
  }

  is_quitting_ = HandleBeforeQuit();
  if (!is_quitting_) {
    return;
  }

  if (WindowList::IsEmpty()) {
    NotifyAndShutdown();
  } else {
    WindowList::CloseAllWindows();
  }
}

void Application::Exit(gin::Arguments* args) {
  int code = 0;
  args->GetNext(&code);

  if (!MainParts::Get()->SetExitCode(code)) {
    // Message loop is not ready, quit directly.
    exit(code);
  } else {
    // Prepare to quit when all windows have been closed.
    is_quitting_ = true;

    // Remember this caller so that we don't emit unrelated events.
    is_exiting_ = true;

    // Must destroy windows before quitting, otherwise bad things can happen.
    if (WindowList::IsEmpty()) {
      Shutdown();
    } else {
      // Unlike Quit(), we do not ask to close window, but destroy the window
      // without asking.
      WindowList::DestroyAllWindows();
    }
  }
}

void Application::Shutdown() {
  if (is_shutdown_) {
    return;
  }

  is_shutdown_ = true;
  is_quitting_ = true;

  observers_.Notify(&ApplicationObserver::OnQuit);

  if (quit_main_message_loop_) {
    RunQuitClosure(std::move(quit_main_message_loop_));
  } else {
    // There is no message loop available so we are in early stage, wait until
    // the quit_main_message_loop_ is available.
    // Exiting now would leave defunct processes behind.
  }
}

std::string Application::GetVersion() const {
  std::string ret = OverriddenApplicationVersion();
  if (ret.empty()) {
    ret = GetExecutableFileVersion();
  }
  return ret;
}

void Application::SetVersion(const std::string& version) {
  OverriddenApplicationVersion() = version;
}

std::string Application::GetName() const {
  std::string ret = OverriddenApplicationName();
  if (ret.empty()) {
    ret = GetExecutableFileProductName();
  }
  return ret;
}

void Application::SetName(const std::string& name) {
  OverriddenApplicationName() = name;
}

bool Application::OpenFile(const std::string& file_path) {
  bool prevent_default = false;
  observers_.Notify(&ApplicationObserver::OnOpenFile, &prevent_default,
                    file_path);
  return prevent_default;
}

void Application::OpenURL(const std::string& url) {
  observers_.Notify(&ApplicationObserver::OnOpenURL, url);
}

void Application::Activate(bool has_visible_windows) {
  observers_.Notify(&ApplicationObserver::OnActivate, has_visible_windows);
}

void Application::WillFinishLaunching() {
  observers_.Notify(&ApplicationObserver::OnWillFinishLaunching);
}

void Application::DidFinishLaunching(base::Value::Dict launch_info) {
  // Make sure the userData directory is created.
  lynxtron::ScopedAllowBlockingForLynxtron allow_blocking;
  base::FilePath user_data;
  if (base::PathService::Get(DIR_USER_DATA, &user_data)) {
    base::CreateDirectoryAndGetError(user_data, nullptr);
#if BUILDFLAG(IS_WIN)
    base::SetExtraNoExecuteAllowedPath(lynxtron::DIR_USER_DATA);
#endif
  }

  is_ready_ = true;
  if (ready_promise_) {
    ready_promise_->Resolve();
  }

  for (ApplicationObserver& observer : observers_) {
    observer.OnFinishLaunching(std::move(launch_info));
  }
}

v8::Local<v8::Value> Application::WhenReady(v8::Isolate* isolate) {
  if (!ready_promise_) {
    ready_promise_ = std::make_unique<gin_helper::Promise<void>>(isolate);
    if (is_ready()) {
      ready_promise_->Resolve();
    }
  }
  return ready_promise_->GetHandle();
}

void Application::OnAccessibilitySupportChanged() {
  observers_.Notify(&ApplicationObserver::OnAccessibilitySupportChanged);
}

void Application::PreMainMessageLoopRun() {
  observers_.Notify(&ApplicationObserver::OnPreMainMessageLoopRun);
}

void Application::PreCreateThreads() {
  observers_.Notify(&ApplicationObserver::OnPreCreateThreads);
}

void Application::SetMainMessageLoopQuitClosure(
    base::OnceClosure quit_closure) {
  if (is_shutdown_) {
    RunQuitClosure(std::move(quit_closure));
  } else {
    quit_main_message_loop_ = std::move(quit_closure);
  }
}

void Application::NotifyAndShutdown() {
  if (is_shutdown_) {
    return;
  }

  bool prevent_default = false;
  observers_.Notify(&ApplicationObserver::OnWillQuit, &prevent_default);
  if (prevent_default) {
    is_quitting_ = false;
    return;
  }

  Shutdown();
}

bool Application::HandleBeforeQuit() {
  bool prevent_default = false;
  observers_.Notify(&ApplicationObserver::OnBeforeQuit, &prevent_default);
  return !prevent_default;
}

void Application::OnWindowCloseCancelled(NativeWindow* window) {
  if (is_quitting_) {
    // Once a beforeunload handler has prevented the closing, we think the
    // quit is cancelled too.
    is_quitting_ = false;
  }
}

void Application::OnWindowAllClosed() {
  if (is_exiting_) {
    Shutdown();
  } else if (is_quitting_) {
    NotifyAndShutdown();
  } else {
    observers_.Notify(&ApplicationObserver::OnWindowAllClosed);
  }
}

#if BUILDFLAG(IS_MAC)
void Application::NewWindowForTab() {
  observers_.Notify(&ApplicationObserver::OnNewWindowForTab);
}

void Application::DidBecomeActive() {
  observers_.Notify(&ApplicationObserver::OnDidBecomeActive);
}

void Application::DidResignActive() {
  observers_.Notify(&ApplicationObserver::OnDidResignActive);
}
#endif

}  // namespace lynxtron
