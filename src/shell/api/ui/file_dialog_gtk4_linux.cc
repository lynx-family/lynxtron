// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <gtk/gtk.h>

#include <memory>

#include "base/functional/bind.h"
#include "gin/dictionary.h"
#include "shell/api/ui/file_dialog.h"
#include "shell/app/native_window.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/common/global_thread.h"
#include "shell/common/linux_desktop.h"
namespace file_dialog {
DialogSettings::DialogSettings() = default;
DialogSettings::DialogSettings(const DialogSettings&) = default;
DialogSettings::~DialogSettings() = default;
namespace {
GtkFileChooserNative* Create(const DialogSettings& settings, bool save) {
  if (!lynxtron::EnsureLinuxDesktop()) {
    return nullptr;
  }
  auto* parent =
      settings.parent_window && !settings.force_detached
          ? GTK_WINDOW(settings.parent_window->GetNativeWindowHandle())
          : nullptr;
  bool directory = !save && (settings.properties & OPEN_DIALOG_OPEN_DIRECTORY);
  auto* dialog = gtk_file_chooser_native_new(
      settings.title.c_str(), parent,
      save        ? GTK_FILE_CHOOSER_ACTION_SAVE
      : directory ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                  : GTK_FILE_CHOOSER_ACTION_OPEN,
      settings.button_label.empty() ? (save ? "_Save" : "_Open")
                                    : settings.button_label.c_str(),
      "_Cancel");
  if (parent) {
    g_signal_connect_object(
        parent, "unrealize", G_CALLBACK(+[](GtkWidget*, gpointer dialog) {
          gtk_native_dialog_hide(GTK_NATIVE_DIALOG(dialog));
          g_signal_emit_by_name(dialog, "response", GTK_RESPONSE_CANCEL);
        }),
        dialog, GConnectFlags(0));
  }
  auto* chooser = GTK_FILE_CHOOSER(dialog);
  gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(dialog), TRUE);
  gtk_file_chooser_set_select_multiple(
      chooser, !save && (settings.properties & OPEN_DIALOG_MULTI_SELECTIONS));
  // GTK4's native/portal chooser owns hidden-file and overwrite preferences.
  if (settings.properties &
      (save ? static_cast<int>(SAVE_DIALOG_SHOW_HIDDEN_FILES)
            : static_cast<int>(OPEN_DIALOG_SHOW_HIDDEN_FILES))) {
    g_warning(
        "GTK4 chooser controls hidden-file visibility through its native UI");
  }
  if (!settings.default_path.empty()) {
    auto path = settings.default_path.value();
    if (g_file_test(path.c_str(), G_FILE_TEST_IS_DIR)) {
      auto* folder = g_file_new_for_path(path.c_str());
      gtk_file_chooser_set_current_folder(chooser, folder, nullptr);
      g_object_unref(folder);
    } else if (save) {
      auto* folder =
          g_file_new_for_path(settings.default_path.DirName().value().c_str());
      gtk_file_chooser_set_current_folder(chooser, folder, nullptr);
      g_object_unref(folder);
      gtk_file_chooser_set_current_name(
          chooser, settings.default_path.BaseName().value().c_str());
    } else {
      auto* file = g_file_new_for_path(path.c_str());
      gtk_file_chooser_set_file(chooser, file, nullptr);
      g_object_unref(file);
    }
  }
  for (const auto& [name, extensions] : settings.filters) {
    auto* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, name.c_str());
    for (const auto& extension : extensions) {
      gtk_file_filter_add_pattern(
          filter, extension == "*" ? "*" : ("*." + extension).c_str());
    }
    gtk_file_chooser_add_filter(chooser, filter);
  }
  return dialog;
}
std::vector<base::FilePath> Selected(GtkFileChooserNative* dialog,
                                     int response) {
  std::vector<base::FilePath> paths;
  if (response != GTK_RESPONSE_ACCEPT) {
    return paths;
  }
  auto* files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(dialog));
  if (!files) {
    return paths;
  }
  for (guint i = 0; i < g_list_model_get_n_items(files); ++i) {
    auto* file = G_FILE(g_list_model_get_item(files, i));
    auto* path = g_file_get_path(file);
    if (path) {
      paths.emplace_back(path);
    }
    g_free(path);
    g_object_unref(file);
  }
  g_object_unref(files);
  return paths;
}
int RunSync(GtkFileChooserNative* dialog) {
  struct Result {
    GMainLoop* loop;
    int response = GTK_RESPONSE_CANCEL;
  } result{g_main_loop_new(nullptr, FALSE)};
  auto handler = g_signal_connect(
      dialog, "response",
      G_CALLBACK(+[](GtkNativeDialog*, int response, gpointer data) {
        auto* result = static_cast<Result*>(data);
        result->response = response;
        g_main_loop_quit(result->loop);
      }),
      &result);
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
  g_main_loop_run(result.loop);
  g_signal_handler_disconnect(dialog, handler);
  g_main_loop_unref(result.loop);
  return result.response;
}
struct Pending {
  gin_helper::Promise<gin_helper::Dictionary> promise;
  bool save;
};
void Resolve(Pending* pending, const std::vector<base::FilePath>& paths) {
  v8::HandleScope handle_scope(pending->promise.isolate());
  v8::Context::Scope context_scope(pending->promise.GetContext());
  auto dict = gin::Dictionary::CreateEmpty(pending->promise.isolate());
  dict.Set("canceled", paths.empty());
  if (pending->save) {
    dict.Set("filePath", paths.empty() ? base::FilePath() : paths[0]);
  } else {
    dict.Set("filePaths", paths);
  }
  pending->promise.Resolve(dict);
}
void PostResolve(Pending* pending, std::vector<base::FilePath> paths) {
  lynxtron::GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<Pending> pending,
                        std::vector<base::FilePath> paths) {
                       Resolve(pending.get(), paths);
                     },
                     std::unique_ptr<Pending>(pending), std::move(paths)));
}
void Async(const DialogSettings& settings,
           gin_helper::Promise<gin_helper::Dictionary> promise,
           bool save) {
  auto* pending = new Pending{std::move(promise), save};
  auto* dialog = Create(settings, save);
  if (!dialog) {
    PostResolve(pending, {});
    return;
  }
  g_signal_connect(
      dialog, "response",
      G_CALLBACK(+[](GtkNativeDialog* dialog, gint response, gpointer user) {
        auto* pending = static_cast<Pending*>(user);
        auto paths = Selected(GTK_FILE_CHOOSER_NATIVE(dialog), response);
        gtk_native_dialog_destroy(dialog);
        g_object_unref(dialog);
        PostResolve(pending, std::move(paths));
      }),
      pending);
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}
}  // namespace
bool ShowOpenDialogSync(const DialogSettings& settings,
                        std::vector<base::FilePath>* paths) {
  auto* dialog = Create(settings, false);
  if (!dialog) {
    return false;
  }
  auto selected = Selected(dialog, RunSync(dialog));
  gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(dialog));
  g_object_unref(dialog);
  *paths = std::move(selected);
  return !paths->empty();
}
void ShowOpenDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  Async(settings, std::move(promise), false);
}
std::optional<base::FilePath> ShowSaveDialogSync(
    const DialogSettings& settings) {
  auto* dialog = Create(settings, true);
  if (!dialog) {
    return std::nullopt;
  }
  auto selected = Selected(dialog, RunSync(dialog));
  gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(dialog));
  g_object_unref(dialog);
  return selected.empty() ? std::nullopt
                          : std::optional<base::FilePath>(selected[0]);
}
void ShowSaveDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  Async(settings, std::move(promise), true);
}
}  // namespace file_dialog
