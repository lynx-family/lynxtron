// Copyright (c) 2025 Lynxtron, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/notifications/windows_toast_notification.h"

#include <shlobj.h>
#include <wrl/wrappers/corewrappers.h>

#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/win/scoped_hstring.h"
#include "shell/api/notifications/notification_delegate.h"
#include "shell/api/notifications/notification_platform_bridge_win.h"
#include "shell/common/application_info.h"
#include "shell/common/global_thread.h"
#include "third_party/libxml/chromium/xml_writer.h"

using ABI::Windows::Data::Xml::Dom::IXmlDocument;
using ABI::Windows::Data::Xml::Dom::IXmlDocumentIO;
using Microsoft::WRL::Wrappers::HStringReference;

namespace winui = ABI::Windows::UI;

#define RETURN_IF_FAILED(hr)                           \
  do {                                                 \
    if (const HRESULT _hrTemp = hr; FAILED(_hrTemp)) { \
      return _hrTemp;                                  \
    }                                                  \
  } while (false)

namespace lynxtron {

namespace {

// This string needs to be max 16 characters to work on Windows 10 prior to
// applying Creators Update (build 15063).
constexpr wchar_t kGroup[] = L"Notifications";

void DebugLog(std::string_view log_msg) {
  // if (lynxtron::debug_notifications)
  LOG(INFO) << log_msg;
}

std::wstring GetTag(const std::string_view notification_id) {
  return base::NumberToWString(base::FastHash(notification_id));
}

// See https://www.hresult.info for HRESULT error codes.
const std::string FailureResultToString(HRESULT failure_reason) {
  // Simplified for now, just return HRESULT
  return base::StrCat(
      {"HRESULT: ", base::NumberToString(static_cast<long>(failure_reason))});
}

constexpr char kPlaceholderContent[] = "placeHolderContent";
constexpr char kContent[] = "content";
constexpr char kToast[] = "toast";
constexpr char kVisual[] = "visual";
constexpr char kBinding[] = "binding";
constexpr char kTemplate[] = "template";
constexpr char kToastTemplate[] = "ToastGeneric";
constexpr char kText[] = "text";
constexpr char kImage[] = "image";
constexpr char kPlacement[] = "placement";
constexpr char kAppLogoOverride[] = "appLogoOverride";
constexpr char kHintCrop[] = "hint-crop";
constexpr char kHintInputId[] = "hint-inputId";
constexpr char kHintCropNone[] = "none";
constexpr char kSrc[] = "src";
constexpr char kAudio[] = "audio";
constexpr char kSilent[] = "silent";
constexpr char kReply[] = "reply";
constexpr char kTrue[] = "true";
constexpr char kID[] = "id";
constexpr char kInput[] = "input";
constexpr char kType[] = "type";
constexpr char kSelection[] = "selection";
constexpr char kScenario[] = "scenario";
constexpr char kReminder[] = "reminder";
constexpr char kActions[] = "actions";
constexpr char kAction[] = "action";
constexpr char kActivationType[] = "activationType";
constexpr char kActivationTypeForeground[] = "foreground";
constexpr char kActivationTypeSystem[] = "system";
constexpr char kArguments[] = "arguments";
constexpr char kLaunch[] = "launch";
constexpr char kDismiss[] = "dismiss";
// The XML version header that has to be stripped from the output.
constexpr char kXmlVersionHeader[] = "<?xml version=\"1.0\"?>\n";

}  // namespace

// static
ComPtr<winui::Notifications::IToastNotificationManagerStatics>*
    WindowsToastNotification::toast_manager_ = nullptr;

// static
ComPtr<winui::Notifications::IToastNotifier>*
    WindowsToastNotification::toast_notifier_ = nullptr;

// static
scoped_refptr<base::SequencedTaskRunner>
WindowsToastNotification::GetToastTaskRunner() {
  // Use function-local static to avoid exit-time destructor warning
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()}));
  return *task_runner;
}

bool WindowsToastNotification::Initialize() {
  HRESULT hr = Windows::Foundation::Initialize(RO_INIT_SINGLETHREADED);
  if (FAILED(hr)) {
    Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
  }

  Microsoft::WRL::Wrappers::HStringReference toast_manager_str(
      RuntimeClass_Windows_UI_Notifications_ToastNotificationManager);

  if (!toast_manager_) {
    toast_manager_ = new ComPtr<
        ABI::Windows::UI::Notifications::IToastNotificationManagerStatics>();
  }

  if (FAILED(Windows::Foundation::GetActivationFactory(
          toast_manager_str.Get(), toast_manager_->GetAddressOf()))) {
    return false;
  }

  if (!toast_notifier_) {
    toast_notifier_ =
        new ComPtr<ABI::Windows::UI::Notifications::IToastNotifier>();
  }

  if (IsRunningInDesktopBridge()) {
    // Ironically, the Desktop Bridge / UWP environment
    // requires us to not give Windows an appUserModelId.
    return SUCCEEDED(
        (*toast_manager_)
            ->CreateToastNotifier(toast_notifier_->GetAddressOf()));
  } else {
    base::win::ScopedHString app_id(nullptr);
    if (!GetAppUserModelID(&app_id)) {
      return false;
    }

    return SUCCEEDED((*toast_manager_)
                         ->CreateToastNotifierWithId(
                             app_id.get(), toast_notifier_->GetAddressOf()));
  }
}

WindowsToastNotification::WindowsToastNotification(
    NotificationDelegate* delegate,
    NotificationOptions* options)
    : Notification(delegate, options) {}

WindowsToastNotification::~WindowsToastNotification() {
  // Remove the notification on exit.
  if (toast_notification_) {
    RemoveCallbacks(toast_notification_.Get());
  }
}

void WindowsToastNotification::Show() {
  DebugLog("WindowsToastNotification::Show called");

  std::string notif_id = notification_id();
  NotificationOptions options_copy = options_;  // Copy options
  base::WeakPtr<Notification> weak_this = weak_factory_.GetWeakPtr();
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      GlobalThread::GetUIThreadTaskRunner();

  DebugLog("Posting task to background thread");
  auto task_runner = GetToastTaskRunner();

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WindowsToastNotification::CreateToastNotificationOnBackgroundThread,
          options_copy, notif_id, weak_this, ui_task_runner));
}

void WindowsToastNotification::CreateToastNotificationOnBackgroundThread(
    const NotificationOptions& options,
    const std::string& notification_id,
    base::WeakPtr<Notification> weak_notification,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DebugLog("CreateToastXmlOnBackgroundThread called");
  ComPtr<IXmlDocument> toast_xml;

  if (!CreateToastXmlDocument(options, notification_id, weak_notification,
                              ui_task_runner, &toast_xml)) {
    return;  // Error already posted to UI thread
  }

  // Continue to create the toast notification
  ComPtr<ABI::Windows::UI::Notifications::IToastNotification>
      toast_notification;
  if (!CreateToastNotification(toast_xml, notification_id, weak_notification,
                               ui_task_runner, &toast_notification)) {
    return;  // Error already posted to UI thread
  }

  // Setup callbacks and show on UI thread (Show must be called on UI thread)
  scoped_refptr<base::SingleThreadTaskRunner> ui_runner =
      GlobalThread::GetUIThreadTaskRunner();
  ui_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowsToastNotification::SetupAndShowOnUIThread,
                     weak_notification, toast_notification));
}

bool WindowsToastNotification::CreateToastXmlDocument(
    const NotificationOptions& options,
    const std::string& notification_id,
    base::WeakPtr<Notification> weak_notification,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    ComPtr<IXmlDocument>* toast_xml) {
  std::wstring icon_path;
  // TODO: Implement icon saving if options.icon is present.

  std::u16string timeout_type = u"default";
  if (options.timeout_type == "never") {
    timeout_type = u"never";
  }

  std::u16string toast_xml_str =
      GetToastXml(notification_id, base::UTF8ToUTF16(options.title),
                  base::UTF8ToUTF16(options.body), icon_path, timeout_type,
                  options.silent, options.actions, options.has_reply,
                  base::UTF8ToUTF16(options.reply_placeholder));

  HRESULT hr = XmlDocumentFromString(base::as_wcstr(toast_xml_str),
                                     toast_xml->GetAddressOf());
  if (FAILED(hr)) {
    std::string err =
        base::StrCat({"XML: Invalid XML, ERROR ", FailureResultToString(hr)});
    DebugLog(err);
    PostNotificationFailedToUIThread(weak_notification, err, ui_task_runner);
    return false;
  }
  return true;
}

bool WindowsToastNotification::CreateToastNotification(
    ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocument> toast_xml,
    const std::string& notification_id,
    base::WeakPtr<Notification> weak_notification,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    ComPtr<ABI::Windows::UI::Notifications::IToastNotification>*
        toast_notification) {
  Microsoft::WRL::Wrappers::HStringReference toast_str(
      RuntimeClass_Windows_UI_Notifications_ToastNotification);

  ComPtr<winui::Notifications::IToastNotificationFactory> toast_factory;
  HRESULT hr = Windows::Foundation::GetActivationFactory(toast_str.Get(),
                                                         &toast_factory);
  if (FAILED(hr)) {
    PostNotificationFailedToUIThread(
        weak_notification, "GetActivationFactory failed", ui_task_runner);
    return false;
  }

  hr = toast_factory->CreateToastNotification(
      toast_xml.Get(), toast_notification->GetAddressOf());
  if (FAILED(hr)) {
    PostNotificationFailedToUIThread(
        weak_notification, "CreateToastNotification failed", ui_task_runner);
    return false;
  }

  ComPtr<winui::Notifications::IToastNotification2> toast2;
  hr = (*toast_notification)->QueryInterface(IID_PPV_ARGS(&toast2));
  if (FAILED(hr)) {
    PostNotificationFailedToUIThread(
        weak_notification, "QueryInterface IToastNotification2 failed",
        ui_task_runner);
    return false;
  }

  Microsoft::WRL::Wrappers::HStringReference group(kGroup);
  hr = toast2->put_Group(group.Get());
  if (FAILED(hr)) {
    PostNotificationFailedToUIThread(weak_notification, "put_Group failed",
                                     ui_task_runner);
    return false;
  }

  // GetTag returns std::wstring, use .c_str() for HStringReference
  std::wstring tag_w = GetTag(notification_id);
  Microsoft::WRL::Wrappers::HStringReference tag(tag_w.c_str());
  hr = toast2->put_Tag(tag.Get());
  if (FAILED(hr)) {
    PostNotificationFailedToUIThread(weak_notification, "put_Tag failed",
                                     ui_task_runner);
    return false;
  }

  return true;
}

void WindowsToastNotification::SetupAndShowOnUIThread(
    base::WeakPtr<Notification> weak_notification,
    ComPtr<ABI::Windows::UI::Notifications::IToastNotification> notification) {
  auto* notif = static_cast<WindowsToastNotification*>(weak_notification.get());
  if (!notif) {
    return;
  }

  HRESULT hr = notif->SetupCallbacks(notification.Get());
  if (FAILED(hr)) {
    if (notif->delegate()) {
      notif->delegate()->NotificationFailed();
    }
    return;
  }

  notif->toast_notification_ = notification;

  hr = (*toast_notifier_)->Show(notification.Get());
  if (FAILED(hr)) {
    if (notif->delegate()) {
      notif->delegate()->NotificationFailed();
    }
    return;
  }

  DebugLog("Notification created");
  if (notif->delegate()) {
    notif->delegate()->NotificationDisplayed();
  }
}

void WindowsToastNotification::PostNotificationFailedToUIThread(
    base::WeakPtr<Notification> weak_notification,
    const std::string& error,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  if (!ui_task_runner) {
    ui_task_runner = GlobalThread::GetUIThreadTaskRunner();
  }
  ui_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<Notification> weak_notification,
                        const std::string& error) {
                       if (!weak_notification) {
                         return;
                       }
                       if (weak_notification->delegate()) {
                         weak_notification->delegate()->NotificationFailed();
                       }
                     },
                     weak_notification, error));
}

void WindowsToastNotification::Dismiss() {
  DebugLog("Hiding notification");
  if (toast_notifier_ && *toast_notifier_ && toast_notification_) {
    (*toast_notifier_)->Hide(toast_notification_.Get());
  }
}

std::u16string WindowsToastNotification::GetToastXml(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& msg,
    const std::wstring& icon_path,
    const std::u16string& timeout_type,
    bool silent,
    const std::vector<NotificationAction>& actions,
    bool has_reply,
    const std::u16string& reply_placeholder) {
  XmlWriter xml_writer;
  xml_writer.StartWriting();

  // <toast ...>
  xml_writer.StartElement(kToast);

  std::string launch_args =
      base::StrCat({"type=click&tag=",
                    base::NumberToString(base::FastHash(notification_id))});
  xml_writer.AddAttribute(kLaunch, launch_args);
  xml_writer.AddAttribute(kActivationType, kActivationTypeForeground);

  const bool is_reminder = (timeout_type == u"never");
  if (is_reminder) {
    xml_writer.AddAttribute(kScenario, kReminder);
  }

  // <visual>
  xml_writer.StartElement(kVisual);
  // <binding template="<template>">
  xml_writer.StartElement(kBinding);
  xml_writer.AddAttribute(kTemplate, kToastTemplate);

  std::u16string line1;
  std::u16string line2;
  if (title.empty() || msg.empty()) {
    line1 = title.empty() ? msg : title;
    if (line1.empty()) {
      line1 = u"[no message]";
    }
    // <text>
    xml_writer.StartElement(kText);
    xml_writer.AppendElementContent(base::UTF16ToUTF8(line1));
    xml_writer.EndElement();  // </text>
  } else {
    line1 = title;
    line2 = msg;
    // <text>
    xml_writer.StartElement(kText);
    xml_writer.AppendElementContent(base::UTF16ToUTF8(line1));
    xml_writer.EndElement();  // </text>
    // <text>
    xml_writer.StartElement(kText);
    xml_writer.AppendElementContent(base::UTF16ToUTF8(line2));
    xml_writer.EndElement();  // </text>
  }

  if (!icon_path.empty()) {
    // <image>
    xml_writer.StartElement(kImage);
    xml_writer.AddAttribute(kID, "1");
    xml_writer.AddAttribute(kPlacement, kAppLogoOverride);
    xml_writer.AddAttribute(kHintCrop, kHintCropNone);
    xml_writer.AddAttribute(kSrc, base::WideToUTF8(icon_path));
    xml_writer.EndElement();  // </image>
  }

  xml_writer.EndElement();  // </binding>
  xml_writer.EndElement();  // </visual>

  if (is_reminder || has_reply || !actions.empty()) {
    // <actions>
    xml_writer.StartElement(kActions);

    // Actions loop
    for (size_t i = 0; i < actions.size(); ++i) {
      const auto& act = actions[i];
      // <action>
      xml_writer.StartElement(kAction);
      xml_writer.AddAttribute(kActivationType, kActivationTypeForeground);
      std::string args = base::StrCat(
          {"type=action&action=", base::NumberToString(i),
           "&tag=", base::NumberToString(base::FastHash(notification_id))});
      xml_writer.AddAttribute(kArguments, args.c_str());
      // act.text is already std::string (UTF-8), no conversion needed
      xml_writer.AddAttribute(kContent, act.text);
      xml_writer.EndElement();  // <action>
    }

    /**
     * Windows not support reply
    if (has_reply) {
      // <input>
      xml_writer.StartElement(kInput);
      xml_writer.AddAttribute(kID, kReply);
      xml_writer.AddAttribute(kType, kText);
      if (!reply_placeholder.empty()) {
        xml_writer.AddAttribute(kPlaceholderContent,
                                base::UTF16ToUTF8(reply_placeholder));
      }
      xml_writer.EndElement();  // </input>

      // <action>
      xml_writer.StartElement(kAction);
      xml_writer.AddAttribute(kActivationType, kActivationTypeForeground);
      std::string args =
          base::StrCat({"type=reply&tag=",
                        base::NumberToString(base::FastHash(notification_id))});
      xml_writer.AddAttribute(kArguments, args.c_str());
      xml_writer.AddAttribute(kContent, "Reply");
      xml_writer.AddAttribute(kHintInputId, kReply);
      xml_writer.EndElement();  // <action>
    }
      */
    xml_writer.EndElement();  // </actions>
  }

  // Silent audio if requested.
  if (silent) {
    xml_writer.StartElement(kAudio);
    xml_writer.AddAttribute(kSilent, kTrue);
    xml_writer.EndElement();  // </audio>
  }

  xml_writer.EndElement();  // </toast>

  xml_writer.StopWriting();
  std::string xml = xml_writer.GetWrittenString();
  if (base::StartsWith(xml, kXmlVersionHeader, base::CompareCase::SENSITIVE)) {
    xml.erase(0, sizeof(kXmlVersionHeader) - 1);
  }

  return base::UTF8ToUTF16(xml);
}

HRESULT WindowsToastNotification::XmlDocumentFromString(
    const wchar_t* xmlString,
    IXmlDocument** doc) {
  ComPtr<IXmlDocument> xmlDoc;
  RETURN_IF_FAILED(Windows::Foundation::ActivateInstance(
      Microsoft::WRL::Wrappers::HStringReference(
          RuntimeClass_Windows_Data_Xml_Dom_XmlDocument)
          .Get(),
      &xmlDoc));

  ComPtr<IXmlDocumentIO> docIO;
  RETURN_IF_FAILED(xmlDoc.As(&docIO));

  RETURN_IF_FAILED(docIO->LoadXml(
      Microsoft::WRL::Wrappers::HStringReference(xmlString).Get()));

  return xmlDoc.CopyTo(doc);
}

HRESULT WindowsToastNotification::SetupCallbacks(
    winui::Notifications::IToastNotification* toast) {
  event_handler_ = Make<ToastEventHandler>(this);
  RETURN_IF_FAILED(
      toast->add_Activated(event_handler_.Get(), &activated_token_));
  RETURN_IF_FAILED(
      toast->add_Dismissed(event_handler_.Get(), &dismissed_token_));
  RETURN_IF_FAILED(toast->add_Failed(event_handler_.Get(), &failed_token_));
  return S_OK;
}

bool WindowsToastNotification::RemoveCallbacks(
    winui::Notifications::IToastNotification* toast) {
  if (FAILED(toast->remove_Activated(activated_token_))) {
    return false;
  }

  if (FAILED(toast->remove_Dismissed(dismissed_token_))) {
    return false;
  }

  return SUCCEEDED(toast->remove_Failed(failed_token_));
}

/*
/ Toast Event Handler
*/
ToastEventHandler::ToastEventHandler(Notification* notification)
    : notification_(notification->GetWeakPtr()) {}

ToastEventHandler::~ToastEventHandler() = default;

IFACEMETHODIMP ToastEventHandler::Invoke(
    winui::Notifications::IToastNotification* sender,
    IInspectable* args) {
  std::wstring arguments_w;

  if (args) {
    Microsoft::WRL::ComPtr<winui::Notifications::IToastActivatedEventArgs>
        activated_args;
    if (SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&activated_args)))) {
      HSTRING args_hs = nullptr;
      if (SUCCEEDED(activated_args->get_Arguments(&args_hs)) && args_hs) {
        UINT32 len = 0;
        const wchar_t* raw = WindowsGetStringRawBuffer(args_hs, &len);
        if (raw && len) {
          arguments_w.assign(raw, len);
        }
      }
    }
  }

  // Check if structured (Activator handled it)
  bool structured = arguments_w.find(L"&tag=") != std::wstring::npos ||
                    arguments_w.find(L"type=action") != std::wstring::npos ||
                    arguments_w.find(L"type=reply") != std::wstring::npos;
  if (structured) {
    return S_OK;
  }

  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<Notification> notification) {
                       if (notification && notification->delegate()) {
                         notification->delegate()->NotificationClick();
                       }
                     },
                     notification_));

  return S_OK;
}

IFACEMETHODIMP ToastEventHandler::Invoke(
    winui::Notifications::IToastNotification* sender,
    ABI::Windows::UI::Notifications::IToastDismissedEventArgs* e) {
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<Notification> notification) {
                       if (notification && notification->delegate()) {
                         notification->delegate()->NotificationClosed();
                       }
                     },
                     notification_));
  return S_OK;
}

IFACEMETHODIMP ToastEventHandler::Invoke(
    winui::Notifications::IToastNotification* sender,
    ABI::Windows::UI::Notifications::IToastFailedEventArgs* e) {
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<Notification> notification) {
                       if (notification && notification->delegate()) {
                         notification->delegate()->NotificationFailed();
                       }
                     },
                     notification_));
  return S_OK;
}

}  // namespace lynxtron
