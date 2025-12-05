// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "plugin/cef/cef_webview.h"

#include <utility>

#include "lynx/base/include/string/string_conversion_win.h"
#include "plugin/cef/cef_webview_client.h"

namespace lynxtron {
namespace plugin {

namespace {
// cspell:disable
static bool strequals(const cef_string_t& a, const cef_string_t& b) {
  if (a.length == b.length) {
    if (a.str == b.str) {
      return true;
    }
    return memcmp(a.str, b.str, a.length) == 0;
  }
  return false;
}

#if OS_WIN
static std::string Utf8(const char16_t* str) {
  return lynx::base::Utf8FromUtf16((wchar_t*)str, wcslen((wchar_t*)str));
}
static std::string Utf8(const cef_string_t& str) {
  return lynx::base::Utf8FromUtf16((wchar_t*)str.str, str.length);
}
#elif OS_MAC
static std::string Utf8(const cef_string_t& s) {
  cef_string_utf8_t cstr;
  memset(&cstr, 0, sizeof(cstr));
  cef_string_utf16_to_utf8(s.str, s.length, &cstr);
  std::string str;
  if (cstr.length > 0) {
    str = std::string(cstr.str, cstr.length);
  }
  cef_string_utf8_clear(&cstr);
  return str;
}
#endif

class GetCookieVisitor : public CefCookieVisitor {
 public:
  GetCookieVisitor(std::function<void(lynx::pub::LynxValue cookies)> callback)
      : callback_(std::move(callback)),
        array_(lynx::pub::LynxValue::kCreateAsArrayTag) {}
  ~GetCookieVisitor() override { callback_(std::move(array_)); }

  bool Visit(const CefCookie& cookie,
             int count,
             int total,
             bool& deleteCookie) override {
    lynx::pub::LynxValue rec(lynx::pub::LynxValue::kCreateAsMapTag);
    rec.SetProperty("name", lynx::pub::LynxValue(Utf8(cookie.name)));
    rec.SetProperty("value", lynx::pub::LynxValue(Utf8(cookie.value)));
    rec.SetProperty("domain", lynx::pub::LynxValue(Utf8(cookie.domain)));
    rec.SetProperty("path", lynx::pub::LynxValue(Utf8(cookie.path)));
    rec.SetProperty("secure", lynx::pub::LynxValue(!!cookie.secure));
    rec.SetProperty("httpOnly", lynx::pub::LynxValue(!!cookie.httponly));
    if (cookie.has_expires) {
      cef_time_t cef_time;
      double seconds;
      cef_time_from_basetime(cookie.expires, &cef_time);
      cef_time_to_doublet(&cef_time, &seconds);
      rec.SetProperty("expirationDate", lynx::pub::LynxValue(seconds * 1000));
    }
    switch (cookie.same_site) {
      case CEF_COOKIE_SAME_SITE_UNSPECIFIED:
        rec.SetProperty("sameSite", lynx::pub::LynxValue("unspecified"));
        break;
      case CEF_COOKIE_SAME_SITE_NO_RESTRICTION:
        rec.SetProperty("sameSite", lynx::pub::LynxValue("no_restriction"));
        break;
      case CEF_COOKIE_SAME_SITE_LAX_MODE:
        rec.SetProperty("sameSite", lynx::pub::LynxValue("lax"));
        break;
      case CEF_COOKIE_SAME_SITE_STRICT_MODE:
        rec.SetProperty("sameSite", lynx::pub::LynxValue("strict"));
        break;
      default:
        break;
    }
    array_.InsertValue(array_.ArrayLength(), std::move(rec));
    return true;
  }

 private:
  std::function<void(lynx::pub::LynxValue cookies)> callback_;
  lynx::pub::LynxValue array_;

  IMPLEMENT_REFCOUNTING(GetCookieVisitor);
};

class DeleteCookieVisitor : public CefCookieVisitor {
 public:
  DeleteCookieVisitor(const std::string name,
                      std::function<void(int deleted)> callback)
      : name_(name), callback_(std::move(callback)) {}
  ~DeleteCookieVisitor() override { callback_(deleted_); }

  bool Visit(const CefCookie& cookie,
             int count,
             int total,
             bool& deleteCookie) override {
    if (name_.empty() || name_ == Utf8(cookie.name)) {
      deleteCookie = true;
      deleted_++;
    }
    return true;
  }

 private:
  std::string name_;
  std::function<void(int cookies)> callback_;
  int deleted_ = 0;

  IMPLEMENT_REFCOUNTING(DeleteCookieVisitor);
};

class SetCookieCallback : public CefSetCookieCallback {
 public:
  SetCookieCallback(std::function<void(bool success)> callback)
      : callback_(callback) {}
  void OnComplete(bool success) override { callback_(success); }

 private:
  std::function<void(bool success)> callback_;

  IMPLEMENT_REFCOUNTING(SetCookieCallback);
};

}  // namespace

// SetCookieMethod
bool SetCookieMethod::valid() const {
  return !url.empty() && !!cookie.name.str;
}

bool SetCookieMethod::equals(const SetCookieMethod& other) const {
  return url == other.url && strequals(cookie.name, other.cookie.name) &&
         strequals(cookie.path, other.cookie.path);
}

// static
SetCookieMethod SetCookieMethod::build(const lynx::pub::LynxValue& attrs) {
  SetCookieMethod method;
  if (attrs.HasProperty("url")) {
    method.url = attrs.GetProperty("url").StdString();
  }
  if (attrs.HasProperty("name")) {
    std::string s = attrs.GetProperty("name").StdString();
    cef_string_from_utf8(s.c_str(), s.size(), &method.cookie.name);
  }
  if (attrs.HasProperty("value")) {
    std::string s = attrs.GetProperty("value").StdString();
    cef_string_from_utf8(s.c_str(), s.size(), &method.cookie.value);
  }
  if (attrs.HasProperty("domain")) {
    std::string s = attrs.GetProperty("domain").StdString();
    cef_string_from_utf8(s.c_str(), s.size(), &method.cookie.domain);
  }
  if (attrs.HasProperty("path")) {
    std::string s = attrs.GetProperty("path").StdString();
    cef_string_from_utf8(s.c_str(), s.size(), &method.cookie.path);
  }
  method.cookie.secure = attrs.GetProperty("secure").Bool();
  method.cookie.httponly = attrs.GetProperty("httpOnly").Bool();
  if (attrs.HasProperty("expirationDate")) {
    cef_time_t cef_time;
    double seconds = attrs.GetProperty("expirationDate").Double() / 1000;
    cef_time_from_doublet(seconds, &cef_time);
    cef_time_to_basetime(&cef_time, &method.cookie.expires);
    method.cookie.has_expires = true;
  }
  if (attrs.HasProperty("sameSite")) {
    std::string s = attrs.GetProperty("sameSite").StdString();
    if (s == "unspecified") {
      method.cookie.same_site = CEF_COOKIE_SAME_SITE_UNSPECIFIED;
    } else if (s == "no_restriction") {
      method.cookie.same_site = CEF_COOKIE_SAME_SITE_NO_RESTRICTION;
    } else if (s == "lax") {
      method.cookie.same_site = CEF_COOKIE_SAME_SITE_LAX_MODE;
    } else if (s == "strict") {
      method.cookie.same_site = CEF_COOKIE_SAME_SITE_STRICT_MODE;
    }
  }
  return method;
}

// CefWebview
bool CEFWebview::OnCreate() {
  return true;
}

void CEFWebview::OnPropertiesChanged(const lynx::pub::LynxValue& attrs,
                                     const lynx::pub::LynxValue& events) {
  if (attrs.HasProperty("initjs")) {
    init_js_ = attrs.GetProperty("initjs").StdString();
  }
  if (attrs.HasProperty("enable-debug")) {
    enable_devtool_ = attrs.GetProperty("enable-debug").Bool();
  }
  if (attrs.HasProperty("cookies")) {
    const auto cookies = attrs.GetProperty("cookies");
    if (cookies.Type() == lynx_value_array) {
      for (size_t i = 0; i < cookies.ArrayLength(); i++) {
        const auto cookie = cookies.GetValueAt(i);
        if (cookie.Type() == lynx_value_map) {
          SetCookieMethod method = SetCookieMethod::build(cookie);
          if (method.valid()) {
            bool exists = false;
            for (const auto& m : set_cookie_methods_) {
              if (m.equals(method)) {
                exists = true;
                break;
              }
            }
            if (!exists) {
              set_cookie_methods_.push_back(std::move(method));
            }
          }
        }
      }
    }
  }
  if (attrs.HasProperty("use-osr")) {
    use_osr_ = attrs.GetProperty("use-osr").Bool();
  }
  if (attrs.HasProperty("max-fps")) {
    fps_ = attrs.GetProperty("max-fps").Int32();
  }
  if (attrs.HasProperty("src")) {
    std::string url = attrs.GetProperty("src").StdString();
    bool navigate = url_ != url;
    url_ = url;
    if (!browser_) {
      SetupClient();
    } else if (navigate) {
      CefRefPtr<CefFrame> main_frame = browser_->GetMainFrame();
      if (main_frame) {
        main_frame->LoadURL(url);
      }
    }
  }
}

// used in bit field buttons to show which buttons are pressed
enum MouseButton {
  kPrimary = 1 << 0,
  kSecondary = 1 << 1,
  kMiddle = 1 << 2,
  kBack = 1 << 3,
  kForward = 1 << 4,
};

void CEFWebview::OnMotionEvent(native_view_motion_event_t* event) {
  int modifiers = event->buttons & (kPrimary | kSecondary | kMiddle);
  if (event->signal_kind == kNativeSignalScroll) {
    OnMouseWheelEvent(event->x, event->y, modifiers, event->deltaX,
                      -event->deltaY);
    return;
  }
  if (event->type == kNativeEventPanZoomUpdate) {
    OnMouseWheelEvent(event->x, event->y, modifiers, event->deltaX,
                      -event->deltaY);
    return;
  }
  switch (event->type) {
    case kNativeEventDown:
    case kNativeEventUp:
      OnMouseClickEvent(event->x, event->y, event->buttons,
                        event->type == kNativeEventUp);
      break;
    default:
    case kNativeEventHover:
      OnMouseMoveEvent(event->x, event->y, modifiers, false);
      break;
  }
}

void CEFWebview::OnFocusChanged(bool focused, bool is_leaf) {
  if (!browser_) {
    return;
  }
  if (CefRefPtr<CefBrowserHost> host = browser_->GetHost()) {
    host->SetFocus(focused);
    if (!focused && client_->IsImeShown()) {
      host->ImeCancelComposition();

      CefMouseEvent mouse_event;
      mouse_event.x = -1;
      mouse_event.y = -1;
      host->SendMouseClickEvent(mouse_event, MBT_LEFT, false, 1);
    }
  }
}

void CEFWebview::OnMethodInvoked(
    const char* method,
    const lynx::pub::LynxValue& attrs,
    std::function<void(int, lynx::pub::LynxValue&&)> callback) {
  if (strcmp(method, "cookies.get") == 0) {
    auto cookie_manager = CefCookieManager::GetGlobalManager(nullptr);
    auto visitor =
        new GetCookieVisitor([callback](lynx::pub::LynxValue cookies) {
          callback(kSuccess, std::move(cookies));
        });
    if (attrs.HasProperty("url")) {
      std::string url = attrs.GetProperty("url").StdString();
      cookie_manager->VisitUrlCookies(url, true, visitor);
    } else {
      cookie_manager->VisitAllCookies(visitor);
    }
    return;
  }
  if (strcmp(method, "cookies.set") == 0) {
    SetCookieMethod method = SetCookieMethod::build(attrs);
    if (!method.valid()) {
      callback(kInvalidParameter,
               lynx::pub::LynxValue(
                   lynx::pub::LynxValue::CreateAsNullTag::kCreateAsNullTag));
      return;
    }
    auto cookie_manager = CefCookieManager::GetGlobalManager(nullptr);
    auto set_callback = new SetCookieCallback([callback](bool success) {
      callback(kSuccess, lynx::pub::LynxValue(success));
    });
    cookie_manager->SetCookie(method.url, method.cookie, set_callback);
  } else if (strcmp(method, "cookies.remove") == 0) {
    std::string name = attrs.GetProperty("name").StdString();
    auto cookie_manager = CefCookieManager::GetGlobalManager(nullptr);
    auto visitor = new DeleteCookieVisitor(name, [callback](int deleted) {
      callback(kSuccess, lynx::pub::LynxValue(deleted));
    });
    if (attrs.HasProperty("url")) {
      std::string url = attrs.GetProperty("url").StdString();
      cookie_manager->VisitUrlCookies(url, false, visitor);
    } else {
      cookie_manager->VisitAllCookies(visitor);
    }
    return;
  }
  if (strcmp(method, "cookies.flushStore") == 0) {
    auto cookie_manager = CefCookieManager::GetGlobalManager(nullptr);
    bool success = cookie_manager->FlushStore(nullptr);
    callback(kSuccess, lynx::pub::LynxValue(success));
    return;
  }
  if (!browser_) {
    callback(kInvalidStateError,
             lynx::pub::LynxValue(
                 lynx::pub::LynxValue::CreateAsNullTag::kCreateAsNullTag));
    return;
  }

  if (strcmp(method, "reload") == 0) {
    client_->SetLoaded(false);
    browser_->Reload();
    callback(kSuccess,
             lynx::pub::LynxValue(
                 lynx::pub::LynxValue::CreateAsNullTag::kCreateAsNullTag));
  } else if (strcmp(method, "eval") == 0) {
    if (attrs.HasProperty("func")) {
      auto main_frame = browser_->GetMainFrame();
      if (main_frame) {
        std::string func = attrs.GetProperty("func").StdString();
        main_frame->ExecuteJavaScript(func, "<host>", 1);
      }
      callback(kSuccess,
               lynx::pub::LynxValue(
                   lynx::pub::LynxValue::CreateAsNullTag::kCreateAsNullTag));
    } else {
      callback(kInvalidParameter,
               lynx::pub::LynxValue(
                   lynx::pub::LynxValue::CreateAsNullTag::kCreateAsNullTag));
    }
  } else {
    callback(kMethodNotFound,
             lynx::pub::LynxValue(
                 lynx::pub::LynxValue::CreateAsNullTag::kCreateAsNullTag));
  }
}

}  // namespace plugin
}  // namespace lynxtron
