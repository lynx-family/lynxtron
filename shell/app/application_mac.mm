// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/app/application.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/mac_util.h"
#include "base/mac/mac_util.mm"
#include "base/strings/sys_string_conversions.h"
#include "shell/api/api_native_image.h"
#include "shell/app/javascript_environment.h"
#include "shell/app/mac/dict_util.h"
#include "shell/app/mac/lynxtron_application.h"
#include "shell/app/mac/lynxtron_application_delegate.h"
#include "shell/app/native_window.h"
#include "shell/app/window_list.h"
#include "shell/common/application_info.h"
#include "shell/common/gin_converters/login_item_settings_converter.h"
#include "shell/common/gin_helper/arguments.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/common/mac/url_conversions.h"
#include "shell/common/platform_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/url_canon.h"

// TODO(Guo Xi): add IS_MAS_BUILD()
#define IS_MAS_BUILD() false
namespace lynxtron {

namespace {

NSString* GetAppPathForProtocol(const GURL& url) {
  NSURL* ns_url = [NSURL
      URLWithString:base::SysUTF8ToNSString(url.possibly_invalid_spec())];
  base::apple::ScopedCFTypeRef<CFErrorRef> out_err;

  base::apple::ScopedCFTypeRef<CFURLRef> openingApp(
      LSCopyDefaultApplicationURLForURL(base::apple::NSToCFPtrCast(ns_url),
                                        kLSRolesAll, out_err.InitializeInto()));

  if (out_err) {
    // likely kLSApplicationNotFoundErr
    return nullptr;
  }

  NSString* app_path = [base::apple::CFToNSPtrCast(openingApp.get()) path];
  return app_path;
}

std::u16string GetAppDisplayNameForProtocol(NSString* app_path) {
  NSString* app_display_name =
      [[NSFileManager defaultManager] displayNameAtPath:app_path];
  return base::SysNSStringToUTF16(app_display_name);
}

bool CheckLoginItemStatus(bool* is_hidden) {
  base::mac::LoginItemsFileList login_items;
  if (!login_items.Initialize()) {
    return false;
  }

  base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForMainApp());
  if (!item.get()) {
    return false;
  }

  if (is_hidden) {
    *is_hidden = base::mac::IsHiddenLoginItem(item.get());
  }

  return true;
}

LoginItemSettings GetLoginItemSettingsDeprecated() {
  LoginItemSettings settings;
  settings.open_at_login = CheckLoginItemStatus(&settings.open_as_hidden);
  settings.restore_state = base::mac::WasLaunchedAsLoginItemRestoreState();
  settings.opened_at_login = base::mac::WasLaunchedAsLoginOrResumeItem();
  settings.opened_as_hidden = base::mac::WasLaunchedAsHiddenLoginItem();
  return settings;
}

}  // namespace

v8::Local<v8::Promise> Application::GetApplicationInfoForProtocol(
    v8::Isolate* isolate,
    const GURL& url) {
  gin_helper::Promise<gin_helper::Dictionary> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();
  auto dict = gin_helper::Dictionary::CreateEmpty(isolate);

  NSString* ns_app_path = GetAppPathForProtocol(url);

  if (!ns_app_path) {
    promise.RejectWithErrorMessage(
        "Unable to retrieve installation path to app");
    return handle;
  }

  std::u16string app_path = base::SysNSStringToUTF16(ns_app_path);
  std::u16string app_display_name = GetAppDisplayNameForProtocol(ns_app_path);
  // #if !BUILDFLAG(IS_NODE_LYNX)
  //   gfx::Image app_icon = GetApplicationIconForProtocol(ns_app_path);
  // #endif

  dict.Set("name", app_display_name);
  dict.Set("path", app_path);
  // #if !BUILDFLAG(IS_NODE_LYNX)
  //   dict.Set("icon", app_icon);
  // #endif

  promise.Resolve(dict);
  return handle;
}

void Application::SetShutdownHandler(base::RepeatingCallback<bool()> handler) {
  [[LynxtronApplication sharedApplication]
      setShutdownHandler:std::move(handler)];
}

void Application::Focus(gin::Arguments* args) {
  gin_helper::Dictionary opts;
  bool steal_focus = false;

  if (args->GetNext(&opts)) {
    gin_helper::ErrorThrower thrower(args->isolate());
    if (!opts.Get("steal", &steal_focus)) {
      thrower.ThrowError(
          "Expected options object to contain a 'steal' boolean property");
      return;
    }
  }

  [[LynxtronApplication sharedApplication]
      activateIgnoringOtherApps:steal_focus];
}

void Application::Hide() {
  [[LynxtronApplication sharedApplication] hide:nil];
}

bool Application::IsHidden() {
  return [[LynxtronApplication sharedApplication] isHidden];
}

void Application::Show() {
  [[LynxtronApplication sharedApplication] unhide:nil];
}

void Application::AddRecentDocument(const base::FilePath& path) {
  NSURL* url = base::apple::FilePathToNSURL(path);
  if (!url) {
    LOG(WARNING) << "Failed to convert file path " << path.value()
                 << " to NSURL";
    return;
  }

  [[NSDocumentController sharedDocumentController]
      noteNewRecentDocumentURL:url];
}

void Application::ClearRecentDocuments() {
  [[NSDocumentController sharedDocumentController] clearRecentDocuments:nil];
}

std::vector<std::string> Application::GetRecentDocuments() {
  NSArray<NSURL*>* recentURLs =
      [[NSDocumentController sharedDocumentController] recentDocumentURLs];
  std::vector<std::string> documents;
  documents.reserve([recentURLs count]);
  for (NSURL* url in recentURLs) {
    documents.emplace_back([url.path UTF8String]);
  }
  return documents;
}

bool Application::RemoveAsDefaultProtocolClient(const std::string& protocol,
                                                gin::Arguments* args) {
  NSString* identifier = [base::apple::MainBundle() bundleIdentifier];
  if (!identifier) {
    return false;
  }

  if (!Application::IsDefaultProtocolClient(protocol, args)) {
    return false;
  }

  NSString* protocol_ns = [NSString stringWithUTF8String:protocol.c_str()];
  CFStringRef protocol_cf = base::apple::NSToCFPtrCast(protocol_ns);
// TODO(codebytere): Use -[NSWorkspace URLForApplicationToOpenURL:] instead
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CFArrayRef bundleList = LSCopyAllHandlersForURLScheme(protocol_cf);
#pragma clang diagnostic pop
  if (!bundleList) {
    return false;
  }
  // On macOS, we can't query the default, but the handlers list seems to put
  // Apple's defaults first, so we'll use the first option that isn't our bundle
  CFStringRef other = nil;
  for (CFIndex i = 0; i < CFArrayGetCount(bundleList); ++i) {
    other =
        base::apple::CFCast<CFStringRef>(CFArrayGetValueAtIndex(bundleList, i));
    if (![identifier isEqualToString:(__bridge NSString*)other]) {
      break;
    }
  }

  // No other app was found set it to none instead of setting it back to itself.
  if ([identifier isEqualToString:(__bridge NSString*)other]) {
    other = base::apple::NSToCFPtrCast(@"None");
  }

  OSStatus return_code = LSSetDefaultHandlerForURLScheme(protocol_cf, other);
  return return_code == noErr;
}

bool Application::SetAsDefaultProtocolClient(const std::string& protocol,
                                             gin::Arguments* args) {
  if (protocol.empty()) {
    return false;
  }

  NSString* identifier = [base::apple::MainBundle() bundleIdentifier];
  if (!identifier) {
    return false;
  }

  NSString* protocol_ns = [NSString stringWithUTF8String:protocol.c_str()];
  OSStatus return_code =
      LSSetDefaultHandlerForURLScheme(base::apple::NSToCFPtrCast(protocol_ns),
                                      base::apple::NSToCFPtrCast(identifier));
  return return_code == noErr;
}

bool Application::IsDefaultProtocolClient(const std::string& protocol,
                                          gin::Arguments* args) {
  if (protocol.empty()) {
    return false;
  }

  NSString* identifier = [base::apple::MainBundle() bundleIdentifier];
  if (!identifier) {
    return false;
  }

  NSString* protocol_ns = [NSString stringWithUTF8String:protocol.c_str()];

// TODO(codebytere): Use -[NSWorkspace URLForApplicationToOpenURL:] instead
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  base::apple::ScopedCFTypeRef<CFStringRef> bundleId(
      LSCopyDefaultHandlerForURLScheme(
          base::apple::NSToCFPtrCast(protocol_ns)));
#pragma clang diagnostic pop
  if (!bundleId) {
    return false;
  }

  // Ensure the comparison is case-insensitive
  // as LS does not persist the case of the bundle id.
  NSComparisonResult result = [base::apple::CFToNSPtrCast(bundleId.get())
      caseInsensitiveCompare:identifier];
  return result == NSOrderedSame;
}

std::u16string Application::GetApplicationNameForProtocol(const GURL& url) {
  NSString* app_path = GetAppPathForProtocol(url);
  if (!app_path) {
    return std::u16string();
  }
  std::u16string app_display_name = GetAppDisplayNameForProtocol(app_path);
  return app_display_name;
}

// bool Application::SetBadgeCount(std::optional<int> count) {
//   DockSetBadgeText(!count.has_value() || count.value() != 0
//                        ? badging::BadgeManager::GetBadgeString(count)
//                        : "");
//   if (count.has_value()) {
//     badge_count_ = count.value();
//   } else {
//     badge_count_ = 0;
//   }
//   return true;
// }

void Application::SetUserActivity(const std::string& type,
                                  base::Value::Dict user_info,
                                  gin::Arguments* args) {
  std::string url_string;
  args->GetNext(&url_string);

  [[LynxtronApplication sharedApplication]
      setCurrentActivity:base::SysUTF8ToNSString(type)
            withUserInfo:DictionaryValueToNSDictionary(std::move(user_info))
          withWebpageURL:net::NSURLWithGURL(GURL(url_string))];
}

std::string Application::GetCurrentActivityType() {
  NSUserActivity* userActivity =
      [[LynxtronApplication sharedApplication] getCurrentActivity];
  return base::SysNSStringToUTF8(userActivity.activityType);
}

void Application::InvalidateCurrentActivity() {
  [[LynxtronApplication sharedApplication] invalidateCurrentActivity];
}

void Application::ResignCurrentActivity() {
  [[LynxtronApplication sharedApplication] resignCurrentActivity];
}

void Application::UpdateCurrentActivity(const std::string& type,
                                        base::Value::Dict user_info) {
  [[LynxtronApplication sharedApplication]
      updateCurrentActivity:base::SysUTF8ToNSString(type)
               withUserInfo:DictionaryValueToNSDictionary(
                                std::move(user_info))];
}

bool Application::WillContinueUserActivity(const std::string& type) {
  bool prevent_default = false;
  for (ApplicationObserver& observer : observers_) {
    observer.OnWillContinueUserActivity(&prevent_default, type);
  }
  return prevent_default;
}

void Application::DidFailToContinueUserActivity(const std::string& type,
                                                const std::string& error) {
  for (ApplicationObserver& observer : observers_) {
    observer.OnDidFailToContinueUserActivity(type, error);
  }
}

bool Application::ContinueUserActivity(const std::string& type,
                                       base::Value::Dict user_info,
                                       base::Value::Dict details) {
  bool prevent_default = false;
  for (ApplicationObserver& observer : observers_) {
    observer.OnContinueUserActivity(&prevent_default, type, user_info.Clone(),
                                    details.Clone());
  }
  return prevent_default;
}

void Application::UserActivityWasContinued(const std::string& type,
                                           base::Value::Dict user_info) {
  for (ApplicationObserver& observer : observers_) {
    observer.OnUserActivityWasContinued(type, user_info.Clone());
  }
}

bool Application::UpdateUserActivityState(const std::string& type,
                                          base::Value::Dict user_info) {
  bool prevent_default = false;
  for (ApplicationObserver& observer : observers_) {
    observer.OnUpdateUserActivityState(&prevent_default, type,
                                       user_info.Clone());
  }
  return prevent_default;
}

// Modified from chrome/browser/ui/cocoa/l10n_util.mm.
// void Application::ApplyForcedRTL() {
//   NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;

//   auto dir = base::i18n::GetForcedTextDirection();

//   // An Electron app should respect RTL behavior of application locale over
//   // system locale.
//   auto should_be_rtl = dir == base::i18n::RIGHT_TO_LEFT || IsAppRTL();
//   auto should_be_ltr = dir == base::i18n::LEFT_TO_RIGHT || !IsAppRTL();

//   // -registerDefaults: won't do the trick here because these defaults exist
//   // (in the global domain) to reflect the system locale. They need to be set
//   // in Chrome's domain to supersede the system value.
//   if (should_be_rtl) {
//     [defaults setBool:YES forKey:@"AppleTextDirection"];
//     [defaults setBool:YES forKey:@"NSForceRightToLeftWritingDirection"];
//   } else if (should_be_ltr) {
//     [defaults setBool:YES forKey:@"AppleTextDirection"];
//     [defaults setBool:NO forKey:@"NSForceRightToLeftWritingDirection"];
//   } else {
//     [defaults removeObjectForKey:@"AppleTextDirection"];
//     [defaults removeObjectForKey:@"NSForceRightToLeftWritingDirection"];
//   }
// }

v8::Local<v8::Value> Application::GetLoginItemSettings(
    const LoginItemSettings& options) {
  LoginItemSettings settings;
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();

  if (options.type != "mainAppService" && options.service_name.empty()) {
    gin_helper::ErrorThrower(isolate).ThrowTypeError(
        "'name' is required when type is not mainAppService");
    return v8::Local<v8::Value>();
  }

#if IS_MAS_BUILD()
  const std::string status =
      platform_util::GetLoginItemEnabled(options.type, options.service_name);
  settings.open_at_login =
      status == "enabled" || status == "enabled-deprecated";
  settings.opened_at_login = was_launched_at_login_;
  if (@available(macOS 13, *)) {
    settings.status = status;
  }
#else
  // If the app was previously set as a LoginItem with the deprecated API,
  // we should report its LoginItemSettings via the old API.
  LoginItemSettings settings_deprecated = GetLoginItemSettingsDeprecated();
  if (@available(macOS 13, *)) {
    const std::string status =
        platform_util::GetLoginItemEnabled(options.type, options.service_name);
    if (status == "enabled-deprecated") {
      settings = settings_deprecated;
    } else {
      settings.open_at_login = status == "enabled";
      settings.opened_at_login = was_launched_at_login_;
      settings.status = status;
    }
  } else {
    settings = settings_deprecated;
  }
#endif
  return gin::ConvertToV8(isolate, settings);
}

void Application::SetLoginItemSettings(LoginItemSettings settings) {
  if (settings.type != "mainAppService" && settings.service_name.empty()) {
    gin_helper::ErrorThrower(JavascriptEnvironment::GetIsolate())
        .ThrowTypeError("'name' is required when type is not mainAppService");
    return;
  }
#if IS_MAS_BUILD()
  platform_util::SetLoginItemEnabled(settings.type, settings.service_name,
                                     settings.open_at_login);
#else
  const base::FilePath bundle_path = base::apple::MainBundlePath();
  if (@available(macOS 13, *)) {
    // If the app was previously set as a LoginItem with the old API, remove it
    // as a LoginItem via the old API before re-enabling with the new API.
    const std::string status =
        platform_util::GetLoginItemEnabled("mainAppService", "");
    if (status == "enabled-deprecated") {
      base::mac::RemoveFromLoginItems(bundle_path);
      if (settings.open_at_login) {
        platform_util::SetLoginItemEnabled(settings.type, settings.service_name,
                                           settings.open_at_login);
      }
    } else {
      platform_util::SetLoginItemEnabled(settings.type, settings.service_name,
                                         settings.open_at_login);
    }
  } else {
    if (settings.open_at_login) {
      base::mac::AddToLoginItems(bundle_path, settings.open_as_hidden);
    } else {
      base::mac::RemoveFromLoginItems(bundle_path);
    }
  }
#endif
}

std::string Application::GetExecutableFileVersion() const {
  return GetApplicationVersion();
}

std::string Application::GetExecutableFileProductName() const {
  return GetApplicationName();
}

int Application::DockBounce(BounceType type) {
  return [[LynxtronApplication sharedApplication]
      requestUserAttention:static_cast<NSRequestUserAttentionType>(type)];
}

void Application::DockCancelBounce(int request_id) {
  [[LynxtronApplication sharedApplication]
      cancelUserAttentionRequest:request_id];
}

void Application::DockSetBadgeText(const std::string& label) {
  NSDockTile* tile = [[LynxtronApplication sharedApplication] dockTile];
  [tile setBadgeLabel:base::SysUTF8ToNSString(label)];
}

void Application::DockDownloadFinished(const std::string& filePath) {
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:@"com.apple.DownloadFileFinished"
                    object:base::SysUTF8ToNSString(filePath)];
}

std::string Application::DockGetBadgeText() {
  NSDockTile* tile = [[LynxtronApplication sharedApplication] dockTile];
  return base::SysNSStringToUTF8([tile badgeLabel]);
}

void Application::DockHide() {
  // Transforming application state from UIElement to Foreground is an
  // asynchronous operation, and unfortunately there is currently no way to know
  // when it is finished.
  // So if we call DockHide => DockShow => DockHide => DockShow in a very short
  // time, we would trigger a bug of macOS that, there would be multiple dock
  // icons of the app left in system.
  // To work around this, we make sure DockHide does nothing if it is called
  // immediately after DockShow. After some experiments, 1 second seems to be
  // a proper interval.
  if (!last_dock_show_.is_null() &&
      base::Time::Now() - last_dock_show_ < base::Seconds(1)) {
    return;
  }

  // TODO(Guo Xi): DockHide
  // for (auto* const& window : WindowList::GetWindows())
  //   [window->GetNativeWindow().GetNativeNSWindow() setCanHide:NO];

  ProcessSerialNumber psn = {0, kCurrentProcess};
  TransformProcessType(&psn, kProcessTransformToUIElementApplication);
}

bool Application::DockIsVisible() {
  // Because DockShow has a slight delay this may not be true immediately
  // after that call.
  return ([[NSRunningApplication currentApplication] activationPolicy] ==
          NSApplicationActivationPolicyRegular);
}

v8::Local<v8::Promise> Application::DockShow(v8::Isolate* isolate) {
  last_dock_show_ = base::Time::Now();
  gin_helper::Promise<void> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();

  BOOL active = [[NSRunningApplication currentApplication] isActive];
  ProcessSerialNumber psn = {0, kCurrentProcess};
  if (active) {
    // Workaround buggy behavior of TransformProcessType.
    // http://stackoverflow.com/questions/7596643/
    NSArray* runningApps = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:@"com.apple.dock"];
    for (NSRunningApplication* app in runningApps) {
      [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
      break;
    }
    __block gin_helper::Promise<void> p = std::move(promise);
    dispatch_time_t one_ms = dispatch_time(DISPATCH_TIME_NOW, USEC_PER_SEC);
    dispatch_after(one_ms, dispatch_get_main_queue(), ^{
      TransformProcessType(&psn, kProcessTransformToForegroundApplication);
      dispatch_time_t one_ms_2 = dispatch_time(DISPATCH_TIME_NOW, USEC_PER_SEC);
      dispatch_after(one_ms_2, dispatch_get_main_queue(), ^{
        [[NSRunningApplication currentApplication]
            activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        p.Resolve();
      });
    });
  } else {
    TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    promise.Resolve();
  }
  return handle;
}

void Application::DockSetMenu(LynxtronMenuModel* model) {
  LynxtronApplicationDelegate* delegate =
      (LynxtronApplicationDelegate*)[NSApp delegate];
  [delegate setApplicationDockMenu:model];
}

void Application::DockSetIcon(v8::Isolate* isolate, v8::Local<v8::Value> icon) {
  gfx::Image image;

  if (!icon->IsNull()) {
    api::NativeImage* native_image = nullptr;
    if (!api::NativeImage::TryConvertNativeImage(isolate, icon,
                                                 &native_image)) {
      return;
    }
    image = native_image->image();
  }

  [[LynxtronApplication sharedApplication]
      setApplicationIconImage:image.AsNSImage()];
}

void Application::ShowAboutPanel() {
  NSDictionary* options = DictionaryValueToNSDictionary(about_panel_options_);

  // Credits must be a NSAttributedString instead of NSString
  NSString* credits = (NSString*)options[@"Credits"];
  if (credits != nil) {
    NSMutableDictionary* mutable_options = [options mutableCopy];
    NSAttributedString* creditString = [[NSAttributedString alloc]
        initWithString:credits
            attributes:@{NSForegroundColorAttributeName : [NSColor textColor]}];

    [mutable_options setValue:creditString forKey:@"Credits"];
    options = [NSDictionary dictionaryWithDictionary:mutable_options];
  }

  [[LynxtronApplication sharedApplication]
      orderFrontStandardAboutPanelWithOptions:options];
}

void Application::SetAboutPanelOptions(base::Value::Dict options) {
  about_panel_options_.clear();

  for (const auto pair : options) {
    std::string key = pair.first;
    if (!key.empty() && pair.second.is_string()) {
      key[0] = base::ToUpperASCII(key[0]);
      about_panel_options_.Set(key, pair.second.Clone());
    }
  }
}

void Application::ShowEmojiPanel() {
  [[LynxtronApplication sharedApplication] orderFrontCharacterPalette:nil];
}

bool Application::IsEmojiPanelSupported() {
  return true;
}

// bool Application::IsSecureKeyboardEntryEnabled() {
// #if BUILDFLAG(IS_NODE_LYNX)
//   return true;
// #else
//   return password_input_enabler_.get() != nullptr;
// #endif
// }

// void Application::SetSecureKeyboardEntryEnabled(bool enabled) {
// #if !BUILDFLAG(IS_NODE_LYNX)
//   if (enabled) {
//     password_input_enabler_ =
//         std::make_unique<ui::ScopedPasswordInputEnabler>();
//   } else {
//     password_input_enabler_.reset();
//   }
// #endif
}  // namespace lynxtron
