// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "tray_icon_win.h"

#include <objbase.h>

#include <windows.h>

#include <shellapi.h>

#include <atomic>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "shell/ui/gfx/icon_util.h"
#include "shell/ui/gfx/image/image.h"
#include "shell/ui/lynxtron_menu_model.h"

namespace lynxtron {

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr wchar_t kTrayWindowClassName[] = L"LynxtronTrayIconWindow";
constexpr UINT_PTR kHoverTimerId = 1;
std::atomic<UINT> g_next_icon_id{1};
UINT g_taskbar_created_message = 0;

LRESULT CALLBACK TrayWindowProc(HWND hwnd,
                                UINT message,
                                WPARAM w_param,
                                LPARAM l_param) {
  if (message == WM_NCCREATE) {
    auto* create_struct = reinterpret_cast<CREATESTRUCT*>(l_param);
    SetWindowLongPtr(hwnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
  }

  TrayIconWin* tray_icon =
      reinterpret_cast<TrayIconWin*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

  if (tray_icon && message == g_taskbar_created_message) {
    tray_icon->ResetIcon();
    return 0;
  }

  if (tray_icon && (message == kTrayMessage || message == WM_TIMER)) {
    tray_icon->HandleTrayMessage(message, w_param, l_param);
    return 0;
  }

  return DefWindowProc(hwnd, message, w_param, l_param);
}

ATOM RegisterTrayWindowClass() {
  static ATOM atom = 0;
  if (atom) {
    return atom;
  }
  WNDCLASSEX window_class = {};
  window_class.cbSize = sizeof(WNDCLASSEX);
  window_class.lpfnWndProc = TrayWindowProc;
  window_class.hInstance = GetModuleHandle(nullptr);
  window_class.lpszClassName = kTrayWindowClassName;
  atom = RegisterClassEx(&window_class);
  return atom;
}

TrayPoint GetCursorPosition() {
  POINT point = {};
  GetCursorPos(&point);
  return TrayPoint{static_cast<int>(point.x), static_cast<int>(point.y)};
}

TrayBounds GetIconBounds(HWND hwnd, UINT icon_id, const std::string& guid) {
  NOTIFYICONIDENTIFIER identifier = {};
  identifier.cbSize = sizeof(NOTIFYICONIDENTIFIER);
  identifier.hWnd = hwnd;
  identifier.uID = icon_id;
  if (!guid.empty()) {
    CLSIDFromString(base::UTF8ToWide(guid).c_str(), &identifier.guidItem);
  }
  RECT rect = {};
  if (Shell_NotifyIconGetRect(&identifier, &rect) == S_OK) {
    return TrayBounds{rect.left, rect.top, rect.right - rect.left,
                      rect.bottom - rect.top};
  }
  TrayPoint point = GetCursorPosition();
  return TrayBounds{point.x, point.y, 0, 0};
}

HMENU BuildMenuFromModel(LynxtronMenuModel* model) {
  HMENU menu = CreatePopupMenu();
  const int item_count = model->GetItemCount();
  for (int i = 0; i < item_count; ++i) {
    if (!model->IsVisibleAt(i)) {
      continue;
    }
    const int command_id = model->GetCommandIdAt(i);
    const auto type = model->GetTypeAt(i);
    if (type == LynxtronMenuModel::TYPE_SEPARATOR) {
      AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
      continue;
    }
    if (type == LynxtronMenuModel::TYPE_SUBMENU) {
      HMENU sub_menu = BuildMenuFromModel(model->GetSubmenuModelAt(i));
      std::wstring label = base::UTF16ToWide(model->GetLabelAt(i));
      AppendMenu(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub_menu),
                 label.c_str());
      continue;
    }

    UINT flags = MF_STRING;
    if (!model->IsEnabledAt(i)) {
      flags |= MF_GRAYED;
    }
    if (model->IsItemCheckedAt(i)) {
      flags |= MF_CHECKED;
    }
    std::wstring label = base::UTF16ToWide(model->GetLabelAt(i));
    AppendMenu(menu, flags, command_id, label.c_str());
  }
  return menu;
}

void ExecuteCommandForModel(LynxtronMenuModel* model, int command_id) {
  auto index = model->GetIndexOfCommandId(command_id);
  if (index.has_value()) {
    model->ActivatedAt(index.value(), 0);
  }
}

}  // namespace

TrayIconWin::TrayIconWin(TrayIconObserver* observer, const std::string& guid)
    : observer_(observer), guid_(guid) {
  RegisterTrayWindowClass();
  if (!g_taskbar_created_message) {
    g_taskbar_created_message = RegisterWindowMessage(L"TaskbarCreated");
  }

  icon_id_ = g_next_icon_id++;
  window_ = CreateWindowEx(0, kTrayWindowClassName, L"", WS_POPUP, 0, 0, 0, 0,
                           0, nullptr, GetModuleHandle(nullptr), this);
}

TrayIconWin::~TrayIconWin() {
  DeleteIcon();
  if (window_) {
    DestroyWindow(window_);
    window_ = nullptr;
  }
}

void TrayIconWin::SetImage(const gfx::Image& image) {
  const SkBitmap* bitmap = image.ToSkBitmap();
  if (!bitmap) {
    return;
  }
  if (icon_) {
    DestroyIcon(icon_);
    icon_ = nullptr;
  }
  auto scoped_icon = IconUtil::CreateHICONFromSkBitmap(*bitmap);
  icon_ = scoped_icon.release();
  if (!added_) {
    AddIcon();
  } else {
    UpdateIcon();
  }
}

void TrayIconWin::SetToolTip(const std::string& tool_tip) {
  if (!added_) {
    return;
  }
  NOTIFYICONDATA data = {};
  data.cbSize = sizeof(NOTIFYICONDATA);
  data.hWnd = window_;
  data.uID = icon_id_;
  data.uFlags = NIF_TIP;
  std::wstring wide_tool_tip = base::UTF8ToWide(tool_tip);
  wcsncpy_s(data.szTip, wide_tool_tip.c_str(), _TRUNCATE);
  Shell_NotifyIcon(NIM_MODIFY, &data);
}

void TrayIconWin::SetContextMenu(LynxtronMenuModel* menu) {
  menu_model_ = menu;
}

void TrayIconWin::PopUpContextMenu(LynxtronMenuModel* menu,
                                   const TrayPoint& position) {
  LynxtronMenuModel* menu_to_use = menu ? menu : menu_model_;
  if (!menu_to_use || !window_) {
    return;
  }
  menu_to_use->MenuWillShow();
  HMENU popup_menu = BuildMenuFromModel(menu_to_use);
  SetForegroundWindow(window_);
  UINT flags = TPM_RIGHTBUTTON | TPM_RETURNCMD;
  int command = TrackPopupMenu(popup_menu, flags, position.x, position.y, 0,
                               window_, nullptr);
  if (command) {
    ExecuteCommandForModel(menu_to_use, command);
  }
  DestroyMenu(popup_menu);
  menu_to_use->MenuWillClose();
  PostMessage(window_, WM_NULL, 0, 0);
}

void TrayIconWin::CloseContextMenu() {
  if (window_) {
    PostMessage(window_, WM_CANCELMODE, 0, 0);
  }
}

TrayBounds TrayIconWin::GetBounds() const {
  if (!window_) {
    return TrayBounds{};
  }
  return GetIconBounds(window_, icon_id_, guid_);
}

std::string TrayIconWin::GetGUID() const {
  return guid_;
}

void TrayIconWin::DisplayBalloon(const TrayBalloonOptions& options) {
  if (!added_) {
    return;
  }
  NOTIFYICONDATA data = {};
  data.cbSize = sizeof(NOTIFYICONDATA);
  data.hWnd = window_;
  data.uID = icon_id_;
  data.uFlags = NIF_INFO;
  std::wstring title = base::UTF16ToWide(options.title);
  std::wstring content = base::UTF16ToWide(options.content);
  wcsncpy_s(data.szInfoTitle, title.c_str(), _TRUNCATE);
  wcsncpy_s(data.szInfo, content.c_str(), _TRUNCATE);

  DWORD info_flags = 0;
  switch (options.icon_type) {
    case TrayBalloonIconType::kNone:
      info_flags = NIIF_NONE;
      break;
    case TrayBalloonIconType::kInfo:
      info_flags = NIIF_INFO;
      break;
    case TrayBalloonIconType::kWarning:
      info_flags = NIIF_WARNING;
      break;
    case TrayBalloonIconType::kError:
      info_flags = NIIF_ERROR;
      break;
    case TrayBalloonIconType::kCustom:
      info_flags = NIIF_USER;
      break;
  }
  if (options.large_icon) {
    info_flags |= NIIF_LARGE_ICON;
  }
  if (options.no_sound) {
    info_flags |= NIIF_NOSOUND;
  }
  if (options.respect_quiet_time) {
    info_flags |= NIIF_RESPECT_QUIET_TIME;
  }
  data.dwInfoFlags = info_flags;
  if (options.icon_type == TrayBalloonIconType::kCustom && options.icon) {
    data.hBalloonIcon = options.icon;
  }
  Shell_NotifyIcon(NIM_MODIFY, &data);
}

void TrayIconWin::RemoveBalloon() {
  if (!added_) {
    return;
  }
  NOTIFYICONDATA data = {};
  data.cbSize = sizeof(NOTIFYICONDATA);
  data.hWnd = window_;
  data.uID = icon_id_;
  data.uFlags = NIF_INFO;
  data.szInfo[0] = L'\0';
  data.szInfoTitle[0] = L'\0';
  Shell_NotifyIcon(NIM_MODIFY, &data);
}

void TrayIconWin::Focus() {
  if (!added_) {
    return;
  }
  NOTIFYICONDATA data = {};
  data.cbSize = sizeof(NOTIFYICONDATA);
  data.hWnd = window_;
  data.uID = icon_id_;
  Shell_NotifyIcon(NIM_SETFOCUS, &data);
}

void TrayIconWin::HandleTrayMessage(UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param) {
  if (message == WM_TIMER) {
    if (w_param != kHoverTimerId) {
      return;
    }
    TrayBounds bounds = GetBounds();
    TrayPoint position = GetCursorPosition();
    if (position.x < bounds.x || position.x > bounds.x + bounds.width ||
        position.y < bounds.y || position.y > bounds.y + bounds.height) {
      KillTimer(window_, kHoverTimerId);
      hovering_ = false;
      if (observer_) {
        observer_->OnMouseLeave(position);
      }
    }
    return;
  }

  if (message != kTrayMessage) {
    return;
  }

  TrayBounds bounds = GetBounds();
  TrayPoint position = GetCursorPosition();
  switch (l_param) {
    case WM_LBUTTONDOWN:
      if (observer_) {
        observer_->OnMouseDown(position);
      }
      return;
    case WM_LBUTTONUP:
      if (observer_) {
        observer_->OnMouseUp(position);
        observer_->OnClick(bounds, position);
      }
      return;
    case WM_LBUTTONDBLCLK:
      if (observer_) {
        observer_->OnDoubleClick(bounds);
      }
      return;
    case WM_RBUTTONDOWN:
      if (observer_) {
        observer_->OnMouseDown(position);
      }
      return;
    case WM_RBUTTONUP:
      if (observer_) {
        observer_->OnMouseUp(position);
        observer_->OnRightClick(bounds);
      }
      if (menu_model_) {
        ShowContextMenu(position);
      }
      return;
    case WM_MBUTTONUP:
      if (observer_) {
        observer_->OnMiddleClick(bounds);
      }
      return;
    case WM_MOUSEMOVE:
      if (!hovering_) {
        hovering_ = true;
        SetTimer(window_, kHoverTimerId, 100, nullptr);
        if (observer_) {
          observer_->OnMouseEnter(position);
        }
      }
      if (observer_) {
        observer_->OnMouseMove(position);
      }
      return;
    case NIN_BALLOONSHOW:
      if (observer_) {
        observer_->OnBalloonShow();
      }
      return;
    case NIN_BALLOONUSERCLICK:
      if (observer_) {
        observer_->OnBalloonClick();
      }
      return;
    case NIN_BALLOONHIDE:
    case NIN_BALLOONTIMEOUT:
      if (observer_) {
        observer_->OnBalloonClosed();
      }
      return;
    default:
      return;
  }
}

void TrayIconWin::AddIcon() {
  if (!window_ || !icon_) {
    return;
  }
  NOTIFYICONDATA data = {};
  data.cbSize = sizeof(NOTIFYICONDATA);
  data.hWnd = window_;
  data.uID = icon_id_;
  data.uFlags = NIF_MESSAGE | NIF_ICON;
  data.uCallbackMessage = kTrayMessage;
  data.hIcon = icon_;
  if (!guid_.empty()) {
    data.uFlags |= NIF_GUID;
    CLSIDFromString(base::UTF8ToWide(guid_).c_str(), &data.guidItem);
  }
  if (Shell_NotifyIcon(NIM_ADD, &data)) {
    added_ = true;
  } else {
    // If we failed to add the icon, it might be because the taskbar hasn't been
    // created yet. We'll try again when we receive the TaskbarCreated message.
    added_ = false;
  }
}

void TrayIconWin::ResetIcon() {
  added_ = false;
  AddIcon();
}

void TrayIconWin::UpdateIcon() {
  if (!added_ || !icon_) {
    return;
  }
  NOTIFYICONDATA data = {};
  data.cbSize = sizeof(NOTIFYICONDATA);
  data.hWnd = window_;
  data.uID = icon_id_;
  data.uFlags = NIF_ICON;
  data.hIcon = icon_;
  Shell_NotifyIcon(NIM_MODIFY, &data);
}

void TrayIconWin::DeleteIcon() {
  if (added_) {
    NOTIFYICONDATA data = {};
    data.cbSize = sizeof(NOTIFYICONDATA);
    data.hWnd = window_;
    data.uID = icon_id_;
    Shell_NotifyIcon(NIM_DELETE, &data);
    added_ = false;
  }

  if (icon_) {
    DestroyIcon(icon_);
    icon_ = nullptr;
  }
}

void TrayIconWin::ShowContextMenu(const TrayPoint& position) {
  PopUpContextMenu(menu_model_, position);
}

}  // namespace lynxtron
