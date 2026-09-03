// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/application.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "shell/common/application_info.h"
#include "url/gurl.h"

namespace lynxtron {

// HarmonyOS fallbacks for cross-platform Application:: methods that are
// declared in application.h without a #if guard but defined ONLY in
// application_mac.mm and application_win.cc. The set was identified by
// diffing application.h declarations against application.cc public defs.
// Each fallback returns the type's natural empty/default until the matching
// OHOS Ability or bundle metadata integration is implemented.
//
// NOT stubbed here:
//   - methods inside `#if BUILDFLAG(IS_MAC)` / `#if BUILDFLAG(IS_WIN)` blocks
//     in application.h - those are not declared on harmony, so no link entry
//     is emitted (e.g. Hide/Show/SetUserActivity/Dock*/SetAppUserModelID/
//     SetUserTasks/UserTask ctor)
//   - SetBadgeCount/GetBadgeCount: declared cross-platform in application.h
//     L168-169 but commented out in both application_mac.mm:316 and
//     application_win.cc:614 with no callers - dead declarations on every
//     platform, so harmony does not break link by also leaving them undefined

void Application::Focus(gin::Arguments* args) {}

void Application::AddRecentDocument(const base::FilePath& path) {}

void Application::ClearRecentDocuments() {}

std::vector<std::string> Application::GetRecentDocuments() {
  return {};
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
  return base::ASCIIToUTF16(GetApplicationName());
}

v8::Local<v8::Promise> Application::GetApplicationInfoForProtocol(
    v8::Isolate* isolate,
    const GURL& url) {
  return v8::Local<v8::Promise>();
}

void Application::SetLoginItemSettings(LoginItemSettings settings) {}

v8::Local<v8::Value> Application::GetLoginItemSettings(
    const LoginItemSettings& options) {
  return v8::Local<v8::Value>();
}

bool Application::IsEmojiPanelSupported() {
  return false;
}

std::string Application::GetExecutableFileVersion() const {
  return GetApplicationVersion();
}

std::string Application::GetExecutableFileProductName() const {
  return GetApplicationName();
}

void Application::ShowAboutPanel() {}

void Application::SetAboutPanelOptions(base::Value::Dict options) {}

}  // namespace lynxtron
