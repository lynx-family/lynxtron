// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/api/ui/file_dialog.h"

#include <gtk/gtk.h>

#include "gin/dictionary.h"
#include "shell/app/native_window.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/common/linux_desktop.h"
namespace file_dialog {
DialogSettings::DialogSettings() = default;
DialogSettings::DialogSettings(const DialogSettings&) = default;
DialogSettings::~DialogSettings() = default;
namespace {
GtkWidget* Create(const DialogSettings& settings, bool save) {
  if (!lynxtron::EnsureLinuxDesktop()) {
    return nullptr;
  }
  auto* parent =
      settings.parent_window && !settings.force_detached
          ? GTK_WINDOW(settings.parent_window->GetNativeWindowHandle())
          : nullptr;
  bool directory = !save && (settings.properties & OPEN_DIALOG_OPEN_DIRECTORY);
  auto* dialog = gtk_file_chooser_dialog_new(
      settings.title.c_str(), parent,
      save        ? GTK_FILE_CHOOSER_ACTION_SAVE
      : directory ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                  : GTK_FILE_CHOOSER_ACTION_OPEN,
      "_Cancel", GTK_RESPONSE_CANCEL,
      settings.button_label.empty() ? (save ? "_Save" : "_Open")
                                    : settings.button_label.c_str(),
      GTK_RESPONSE_ACCEPT, nullptr);
  auto* chooser = GTK_FILE_CHOOSER(dialog);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_file_chooser_set_select_multiple(
      chooser, !save && (settings.properties & OPEN_DIALOG_MULTI_SELECTIONS));
  gtk_file_chooser_set_show_hidden(
      chooser, settings.properties &
                   (save ? static_cast<int>(SAVE_DIALOG_SHOW_HIDDEN_FILES)
                         : static_cast<int>(OPEN_DIALOG_SHOW_HIDDEN_FILES)));
  gtk_file_chooser_set_do_overwrite_confirmation(
      chooser,
      save && (settings.properties & SAVE_DIALOG_SHOW_OVERWRITE_CONFIRMATION));
  if (!settings.default_path.empty()) {
    auto path = settings.default_path.value();
    if (g_file_test(path.c_str(), G_FILE_TEST_IS_DIR)) {
      gtk_file_chooser_set_current_folder(chooser, path.c_str());
    } else if (save) {
      gtk_file_chooser_set_current_folder(
          chooser, settings.default_path.DirName().value().c_str());
      gtk_file_chooser_set_current_name(
          chooser, settings.default_path.BaseName().value().c_str());
    } else {
      gtk_file_chooser_set_filename(chooser, path.c_str());
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
std::vector<base::FilePath> Selected(GtkWidget* dialog, int response) {
  std::vector<base::FilePath> paths;
  if (response != GTK_RESPONSE_ACCEPT) {
    return paths;
  }
  auto* filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
  for (auto* entry = filenames; entry; entry = entry->next) {
    paths.emplace_back(static_cast<char*>(entry->data));
  }
  g_slist_free_full(filenames, g_free);
  return paths;
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
void Async(const DialogSettings& settings,
           gin_helper::Promise<gin_helper::Dictionary> promise,
           bool save) {
  auto* pending = new Pending{std::move(promise), save};
  auto* dialog = Create(settings, save);
  if (!dialog) {
    Resolve(pending, {});
    delete pending;
    return;
  }
  g_signal_connect(
      dialog, "response",
      G_CALLBACK(+[](GtkDialog* dialog, gint response, gpointer user) {
        auto* pending = static_cast<Pending*>(user);
        auto paths = Selected(GTK_WIDGET(dialog), response);
        gtk_widget_destroy(GTK_WIDGET(dialog));
        Resolve(pending, paths);
        delete pending;
      }),
      pending);
  gtk_widget_show(dialog);
}
}  // namespace
bool ShowOpenDialogSync(const DialogSettings& settings,
                        std::vector<base::FilePath>* paths) {
  auto* dialog = Create(settings, false);
  if (!dialog) {
    return false;
  }
  auto selected = Selected(dialog, gtk_dialog_run(GTK_DIALOG(dialog)));
  gtk_widget_destroy(dialog);
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
  auto selected = Selected(dialog, gtk_dialog_run(GTK_DIALOG(dialog)));
  gtk_widget_destroy(dialog);
  return selected.empty() ? std::nullopt
                          : std::optional<base::FilePath>(selected[0]);
}
void ShowSaveDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  Async(settings, std::move(promise), true);
}
}  // namespace file_dialog
