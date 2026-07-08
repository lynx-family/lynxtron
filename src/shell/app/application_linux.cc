// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/application.h"

#include "base/files/file_path.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/native_window.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_helper/dictionary.h"

namespace lynxtron {

void Application::Focus(gin::Arguments* args) {
  for (auto* const window : WindowList::GetWindows()) {
    if (window->IsVisible()) {
      window->Focus(true);
      break;
    }
  }
}

void Application::AddRecentDocument(const base::FilePath& path) {}

void Application::ClearRecentDocuments() {}

std::vector<std::string> Application::GetRecentDocuments() {
  return {};
}

std::string Application::GetExecutableFileVersion() const {
  return {};
}

std::string Application::GetExecutableFileProductName() const {
  return GetName();
}

void Application::SetLoginItemSettings(LoginItemSettings settings) {}

v8::Local<v8::Value> Application::GetLoginItemSettings(
    const LoginItemSettings& options) {
  gin_helper::Dictionary dict =
      gin_helper::Dictionary::CreateEmpty(JavascriptEnvironment::GetIsolate());
  dict.Set("openAtLogin", false);
  dict.Set("openAsHidden", false);
  dict.Set("wasOpenedAtLogin", false);
  dict.Set("wasOpenedAsHidden", false);
  dict.Set("restoreState", false);
  return dict.GetHandle();
}

bool Application::RemoveAsDefaultProtocolClient(const std::string& protocol,
                                                gin::Arguments* args) {
  return false;
}

bool Application::SetAsDefaultProtocolClient(const std::string& protocol,
                                             gin::Arguments* args) {
  return false;
}

bool Application::IsDefaultProtocolClient(const std::string& protocol,
                                          gin::Arguments* args) {
  return false;
}

std::u16string Application::GetApplicationNameForProtocol(const GURL& url) {
  return {};
}

v8::Local<v8::Promise> Application::GetApplicationInfoForProtocol(
    v8::Isolate* isolate,
    const GURL& url) {
  gin_helper::Promise<gin_helper::Dictionary> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();
  promise.RejectWithErrorMessage(
      "Application protocol lookup is not implemented on Linux");
  return handle;
}

void Application::ShowAboutPanel() {}

void Application::SetAboutPanelOptions(base::Value::Dict options) {}

bool Application::IsEmojiPanelSupported() {
  return false;
}

}  // namespace lynxtron
