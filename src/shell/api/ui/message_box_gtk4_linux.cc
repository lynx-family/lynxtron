// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include <gtk/gtk.h>

#include <iostream>
#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "shell/api/ui/message_box.h"
#include "shell/app/native_window.h"
#include "shell/common/global_thread.h"
#include "shell/common/linux_desktop.h"
namespace lynxtron {
MessageBoxSettings::MessageBoxSettings() = default;
MessageBoxSettings::MessageBoxSettings(const MessageBoxSettings&) = default;
MessageBoxSettings::~MessageBoxSettings() = default;
namespace {
std::map<int, GtkWidget*> dialogs;
struct Pending {
  MessageBoxCallback callback;
  int cancel;
  std::optional<int> id;
  GtkWidget* checkbox;
};
GtkWidget* Create(const MessageBoxSettings& settings, GtkWidget** checkbox) {
  if (!EnsureLinuxDesktop()) {
    return nullptr;
  }
  auto* parent =
      settings.parent_window
          ? GTK_WINDOW(settings.parent_window->GetNativeWindowHandle())
          : nullptr;
  GtkMessageType type = GTK_MESSAGE_OTHER;
  switch (settings.type) {
    case MessageBoxType::kInformation:
      type = GTK_MESSAGE_INFO;
      break;
    case MessageBoxType::kWarning:
      type = GTK_MESSAGE_WARNING;
      break;
    case MessageBoxType::kError:
      type = GTK_MESSAGE_ERROR;
      break;
    case MessageBoxType::kQuestion:
      type = GTK_MESSAGE_QUESTION;
      break;
    default:
      break;
  }
  auto* dialog =
      gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_NONE,
                             "%s", settings.message.c_str());
  gtk_window_set_title(GTK_WINDOW(dialog), settings.title.c_str());
  if (parent) {
    g_signal_connect_object(
        parent, "unrealize", G_CALLBACK(+[](GtkWidget*, gpointer dialog) {
          gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
        }),
        dialog, GConnectFlags(0));
  }
  if (!settings.detail.empty()) {
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s",
                                             settings.detail.c_str());
  }
  for (size_t i = 0; i < settings.buttons.size(); ++i) {
    gtk_dialog_add_button(GTK_DIALOG(dialog), settings.buttons[i].c_str(), i);
  }
  if (settings.buttons.empty()) {
    gtk_dialog_add_button(GTK_DIALOG(dialog), "OK", 0);
  }
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), settings.default_id);
  *checkbox = nullptr;
  if (!settings.checkbox_label.empty()) {
    *checkbox =
        gtk_check_button_new_with_label(settings.checkbox_label.c_str());
    gtk_check_button_set_active(GTK_CHECK_BUTTON(*checkbox),
                                settings.checkbox_checked);
    gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                   *checkbox);
    gtk_widget_show(*checkbox);
  }
  if (auto* button = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog),
                                                        settings.default_id)) {
    gtk_window_set_focus(GTK_WINDOW(dialog), button);
  }
  return dialog;
}
}  // namespace
int ShowMessageBoxSync(const MessageBoxSettings& settings) {
  GtkWidget* checkbox;
  auto* dialog = Create(settings, &checkbox);
  if (!dialog) {
    return settings.cancel_id;
  }
  struct Result {
    GMainLoop* loop;
    int response = GTK_RESPONSE_CANCEL;
  } result{g_main_loop_new(nullptr, FALSE)};
  auto handler =
      g_signal_connect(dialog, "response",
                       G_CALLBACK(+[](GtkDialog*, int response, gpointer data) {
                         auto* result = static_cast<Result*>(data);
                         result->response = response;
                         g_main_loop_quit(result->loop);
                       }),
                       &result);
  gtk_window_present(GTK_WINDOW(dialog));
  g_main_loop_run(result.loop);
  g_signal_handler_disconnect(dialog, handler);
  g_main_loop_unref(result.loop);
  int response = result.response;
  gtk_window_destroy(GTK_WINDOW(dialog));
  return response < 0 ? settings.cancel_id : response;
}
void ShowMessageBox(const MessageBoxSettings& settings,
                    MessageBoxCallback callback) {
  GtkWidget* checkbox;
  auto* dialog = Create(settings, &checkbox);
  if (!dialog) {
    std::move(callback).Run(settings.cancel_id, settings.checkbox_checked);
    return;
  }
  if (settings.id && dialogs.contains(*settings.id)) {
    CloseMessageBox(*settings.id);
  }
  auto* pending = new Pending{std::move(callback), settings.cancel_id,
                              settings.id, checkbox};
  if (settings.id) {
    dialogs[*settings.id] = dialog;
  }
  g_signal_connect(
      dialog, "response",
      G_CALLBACK(+[](GtkDialog* dialog, gint response, gpointer user) {
        auto* pending = static_cast<Pending*>(user);
        bool checked =
            pending->checkbox &&
            gtk_check_button_get_active(GTK_CHECK_BUTTON(pending->checkbox));
        if (pending->id) {
          dialogs.erase(*pending->id);
        }
        gtk_window_destroy(GTK_WINDOW(dialog));
        auto callback = std::move(pending->callback);
        int cancel = pending->cancel;
        delete pending;
        GlobalThread::GetUIThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           response < 0 ? cancel : response, checked));
      }),
      pending);
  gtk_window_present(GTK_WINDOW(dialog));
}
void CloseMessageBox(int id) {
  auto it = dialogs.find(id);
  if (it != dialogs.end()) {
    gtk_dialog_response(GTK_DIALOG(it->second), GTK_RESPONSE_CANCEL);
  }
}
void ShowErrorBox(const std::u16string& title, const std::u16string& content) {
  if (!EnsureLinuxDesktop()) {
    std::cerr << base::UTF16ToUTF8(title) << ": " << base::UTF16ToUTF8(content)
              << '\n';
    return;
  }
  MessageBoxSettings settings;
  settings.title = base::UTF16ToUTF8(title);
  settings.message = base::UTF16ToUTF8(content);
  settings.type = MessageBoxType::kError;
  settings.default_id = 0;
  settings.cancel_id = 0;
  ShowMessageBoxSync(settings);
}
}  // namespace lynxtron
