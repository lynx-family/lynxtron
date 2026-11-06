// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/application.h"

#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/native_window.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/linux_desktop.h"
#include "url/gurl.h"

namespace lynxtron {

void Application::Focus(gin::Arguments* args) {
  for (auto* const window : WindowList::GetWindows()) {
    if (window->IsVisible()) {
      window->Focus(true);
      break;
    }
  }
}

void Application::AddRecentDocument(const base::FilePath& path) {
  auto* uri = g_filename_to_uri(path.value().c_str(), nullptr, nullptr);
  if (!uri) {
    return;
  }
  gtk_recent_manager_add_item(gtk_recent_manager_get_default(), uri);
  g_free(uri);
}

void Application::ClearRecentDocuments() {
  auto* manager = gtk_recent_manager_get_default();
  auto* items = gtk_recent_manager_get_items(manager);
  for (auto* item = items; item; item = item->next) {
    auto* info = static_cast<GtkRecentInfo*>(item->data);
    if (gtk_recent_info_has_application(info, g_get_application_name())) {
      gtk_recent_manager_remove_item(manager, gtk_recent_info_get_uri(info),
                                     nullptr);
    }
  }
  g_list_free_full(items,
                   reinterpret_cast<GDestroyNotify>(gtk_recent_info_unref));
}

std::vector<std::string> Application::GetRecentDocuments() {
  std::vector<std::string> result;
  auto* items = gtk_recent_manager_get_items(gtk_recent_manager_get_default());
  for (auto* item = items; item; item = item->next) {
    auto* info = static_cast<GtkRecentInfo*>(item->data);
    if (!gtk_recent_info_has_application(info, g_get_application_name())) {
      continue;
    }
    auto* filename =
        g_filename_from_uri(gtk_recent_info_get_uri(info), nullptr, nullptr);
    if (filename) {
      result.emplace_back(filename);
      g_free(filename);
    }
  }
  g_list_free_full(items,
                   reinterpret_cast<GDestroyNotify>(gtk_recent_info_unref));
  return result;
}

std::string Application::GetExecutableFileVersion() const {
  return {};
}

std::string Application::GetExecutableFileProductName() const {
  return GetName();
}

namespace {
std::string RegisteredExecutable(GAppInfo* info) {
  int count = 0;
  char** argv = nullptr;
  if (!g_shell_parse_argv(g_app_info_get_commandline(info), &count, &argv,
                          nullptr)) {
    return {};
  }
  // GIO validates the executable token before expanding desktop field codes.
  // Percent-bearing paths therefore use env as a stable executable token.
  int executable_index = 0;
  if (count > 2 && std::string(argv[0]) == "/usr/bin/env" &&
      std::string(argv[1]) == "--") {
    executable_index = 2;
  }
  std::string executable = count ? argv[executable_index] : "";
  // Exec encodes a literal percent as %% before command-line tokenization.
  // Shell parsing handles quoting, but does not expand desktop field codes.
  for (size_t i = 0; i + 1 < executable.size(); ++i) {
    if (executable[i] == '%' && executable[i + 1] == '%') {
      executable.erase(i, 1);
    }
  }
  g_strfreev(argv);
  return executable;
}
base::FilePath AutostartPath(const std::string& name) {
  auto* escaped = g_uri_escape_string(name.c_str(), nullptr, FALSE);
  base::FilePath result =
      base::FilePath(g_get_user_config_dir())
          .Append("autostart")
          .Append(std::string("lynxtron-") + escaped + ".desktop");
  g_free(escaped);
  return result;
}
std::string QuoteExec(const std::string& value) {
  std::string result = "\"";
  for (char c : value) {
    if (c == '%') {
      result += '%';
    }
    if (c == '\\' || c == '\"' || c == '`' || c == '$') {
      result += '\\';
    }
    result += c;
  }
  return result + "\"";
}
std::string ExecutableCommand(const std::string& path) {
  // Preserve Desktop Entry %% expansion while avoiding GIO's initial
  // executable existence check against a still-escaped percent path.
  return (path.find('%') != std::string::npos ? "/usr/bin/env -- " : "") +
         QuoteExec(path);
}
bool RemoveDefaultAssociation(GAppInfo* info, const std::string& type) {
  auto* filename =
      g_build_filename(g_get_user_config_dir(), "mimeapps.list", nullptr);
  auto* file = g_file_new_for_path(filename);
  g_free(filename);
  gchar* contents = nullptr;
  gchar* etag = nullptr;
  gsize length = 0;
  GError* error = nullptr;
  bool result =
      g_file_load_contents(file, nullptr, &contents, &length, &etag, &error);
  if (!result) {
    result = g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_clear_error(&error);
    g_object_unref(file);
    return result;
  }
  auto* key = g_key_file_new();
  result = g_key_file_load_from_data(
      key, contents, length,
      GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS),
      &error);
  g_free(contents);
  if (result) {
    gsize count = 0;
    auto** entries = g_key_file_get_string_list(key, "Default Applications",
                                                type.c_str(), &count, nullptr);
    std::vector<const gchar*> retained;
    const char* id = g_app_info_get_id(info);
    for (gsize i = 0; i < count; ++i) {
      if (g_strcmp0(entries[i], id) != 0) {
        retained.push_back(entries[i]);
      }
    }
    if (retained.size() != count) {
      if (retained.empty()) {
        g_key_file_remove_key(key, "Default Applications", type.c_str(),
                              nullptr);
      } else {
        g_key_file_set_string_list(key, "Default Applications", type.c_str(),
                                   retained.data(), retained.size());
      }
      auto* updated = g_key_file_to_data(key, &length, nullptr);
      // Refuse to overwrite concurrent changes to unrelated associations.
      result =
          g_file_replace_contents(file, updated, length, etag, FALSE,
                                  G_FILE_CREATE_NONE, nullptr, nullptr, &error);
      g_free(updated);
    }
    g_strfreev(entries);
  }
  g_clear_error(&error);
  g_free(etag);
  g_key_file_unref(key);
  g_object_unref(file);
  return result;
}
}  // namespace
void Application::SetLoginItemSettings(LoginItemSettings settings) {
  auto path = AutostartPath(GetName());
  if (!settings.open_at_login) {
    base::DeleteFile(path);
    return;
  }
  base::FilePath executable;
  if (!settings.path.empty()) {
    executable = base::FilePath(base::UTF16ToUTF8(settings.path));
  } else {
    base::PathService::Get(base::FILE_EXE, &executable);
  }
  std::string command = ExecutableCommand(executable.value());
  for (const auto& arg : settings.args) {
    command += " " + QuoteExec(base::UTF16ToUTF8(arg));
  }
  auto* key = g_key_file_new();
  g_key_file_set_string(key, "Desktop Entry", "Type", "Application");
  g_key_file_set_string(key, "Desktop Entry", "Name", GetName().c_str());
  g_key_file_set_string(key, "Desktop Entry", "Exec", command.c_str());
  gsize length;
  auto* content = g_key_file_to_data(key, &length, nullptr);
  if (base::CreateDirectory(path.DirName())) {
    base::WriteFile(path, std::string_view(content, length));
  }
  g_free(content);
  g_key_file_unref(key);
}

v8::Local<v8::Value> Application::GetLoginItemSettings(
    const LoginItemSettings& options) {
  gin_helper::Dictionary dict =
      gin_helper::Dictionary::CreateEmpty(JavascriptEnvironment::GetIsolate());
  dict.Set("openAtLogin", base::PathExists(AutostartPath(GetName())));
  dict.Set("openAsHidden", false);
  dict.Set("wasOpenedAtLogin", false);
  dict.Set("wasOpenedAsHidden", false);
  dict.Set("restoreState", false);
  return dict.GetHandle();
}

bool Application::RemoveAsDefaultProtocolClient(const std::string& protocol,
                                                gin::Arguments* args) {
  auto* info = g_app_info_get_default_for_uri_scheme(protocol.c_str());
  if (!info) {
    return false;
  }
  std::string path;
  args->GetNext(&path);
  if (path.empty()) {
    base::FilePath exe;
    base::PathService::Get(base::FILE_EXE, &exe);
    path = exe.value();
  }
  bool ours = path == RegisteredExecutable(info);
  const std::string type = "x-scheme-handler/" + protocol;
  bool result = ours && RemoveDefaultAssociation(info, type) &&
                g_app_info_remove_supports_type(info, type.c_str(), nullptr);
  g_object_unref(info);
  return result;
}

bool Application::SetAsDefaultProtocolClient(const std::string& protocol,
                                             gin::Arguments* args) {
  if (protocol.empty() || !g_ascii_isalpha(protocol[0]) ||
      protocol.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS"
                                 "TUVWXYZ0123456789+.-") != std::string::npos) {
    return false;
  }
  std::string path;
  std::vector<std::string> extra;
  args->GetNext(&path);
  args->GetNext(&extra);
  if (path.empty()) {
    base::FilePath exe;
    base::PathService::Get(base::FILE_EXE, &exe);
    path = exe.value();
  }
  std::string command = ExecutableCommand(path);
  for (const auto& arg : extra) {
    command += " " + QuoteExec(arg);
  }
  GError* error = nullptr;
  auto* info = g_app_info_create_from_commandline(
      command.c_str(), GetName().c_str(), G_APP_INFO_CREATE_SUPPORTS_URIS,
      &error);
  bool result =
      info && g_app_info_set_as_default_for_type(
                  info, ("x-scheme-handler/" + protocol).c_str(), &error);
  if (info) {
    g_object_unref(info);
  }
  if (error) {
    g_error_free(error);
  }
  return result;
}

bool Application::IsDefaultProtocolClient(const std::string& protocol,
                                          gin::Arguments* args) {
  auto* info = g_app_info_get_default_for_uri_scheme(protocol.c_str());
  if (!info) {
    return false;
  }
  std::string path;
  args->GetNext(&path);
  if (path.empty()) {
    base::FilePath exe;
    base::PathService::Get(base::FILE_EXE, &exe);
    path = exe.value();
  }
  bool result = path == RegisteredExecutable(info);
  g_object_unref(info);
  return result;
}

std::u16string Application::GetApplicationNameForProtocol(const GURL& url) {
  auto* info = g_app_info_get_default_for_uri_scheme(url.scheme().c_str());
  if (!info) {
    return {};
  }
  auto name = base::UTF8ToUTF16(g_app_info_get_display_name(info));
  g_object_unref(info);
  return name;
}

v8::Local<v8::Promise> Application::GetApplicationInfoForProtocol(
    v8::Isolate* isolate,
    const GURL& url) {
  gin_helper::Promise<gin_helper::Dictionary> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();
  auto* info = g_app_info_get_default_for_uri_scheme(url.scheme().c_str());
  if (!info) {
    promise.RejectWithErrorMessage("No application registered for protocol");
    return handle;
  }
  auto dict = gin_helper::Dictionary::CreateEmpty(isolate);
  dict.Set("name", std::string(g_app_info_get_display_name(info)));
  dict.Set("path", base::FilePath(RegisteredExecutable(info)));
  promise.Resolve(dict);
  g_object_unref(info);
  return handle;
}

void Application::ShowAboutPanel() {
  if (!EnsureLinuxDesktop()) {
    return;
  }
  auto* dialog = gtk_about_dialog_new();
  gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog),
                                    GetName().c_str());
  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), GetVersion().c_str());
  if (auto* copyright = about_panel_options_.FindString("copyright")) {
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog),
                                   copyright->c_str());
  }
  g_signal_connect(dialog, "response",
                   G_CALLBACK(+[](GtkDialog* dialog, gint, gpointer) {
#if LYNXTRON_USE_GTK4
                     gtk_window_destroy(GTK_WINDOW(dialog));
#else
                     gtk_widget_destroy(GTK_WIDGET(dialog));
#endif
                   }),
                   nullptr);
#if LYNXTRON_USE_GTK4
  gtk_window_present(GTK_WINDOW(dialog));
#else
  gtk_widget_show(dialog);
#endif
}

void Application::SetAboutPanelOptions(base::Value::Dict options) {
  about_panel_options_ = std::move(options);
}

bool Application::IsEmojiPanelSupported() {
  return false;
}

}  // namespace lynxtron
