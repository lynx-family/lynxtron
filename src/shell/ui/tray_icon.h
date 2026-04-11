// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_UI_TRAY_ICON_H_
#define LYNXTRON_SHELL_UI_TRAY_ICON_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace gfx {
class Image;
}

namespace lynxtron {

class LynxtronMenuModel;

struct TrayPoint {
  int x = 0;
  int y = 0;
};

struct TrayBounds {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

enum class TrayBalloonIconType {
  kNone,
  kInfo,
  kWarning,
  kError,
  kCustom,
};

struct TrayBalloonOptions {
  std::u16string title;
  std::u16string content;
  TrayBalloonIconType icon_type = TrayBalloonIconType::kCustom;
  bool large_icon = true;
  bool no_sound = false;
  bool respect_quiet_time = false;
#if BUILDFLAG(IS_WIN)
  HICON icon = nullptr;
#endif
};

class TrayIconObserver {
 public:
  virtual ~TrayIconObserver() = default;
  virtual void OnClick(const TrayBounds& bounds, const TrayPoint& position) {}
  virtual void OnRightClick(const TrayBounds& bounds) {}
  virtual void OnDoubleClick(const TrayBounds& bounds) {}
  virtual void OnMiddleClick(const TrayBounds& bounds) {}
  virtual void OnMouseDown(const TrayPoint& position) {}
  virtual void OnMouseUp(const TrayPoint& position) {}
  virtual void OnMouseEnter(const TrayPoint& position) {}
  virtual void OnMouseLeave(const TrayPoint& position) {}
  virtual void OnMouseMove(const TrayPoint& position) {}
  virtual void OnBalloonShow() {}
  virtual void OnBalloonClick() {}
  virtual void OnBalloonClosed() {}
  virtual void OnDrop() {}
  virtual void OnDropFiles(const std::vector<std::string>& files) {}
  virtual void OnDropText(const std::string& text) {}
  virtual void OnDragEnter() {}
  virtual void OnDragLeave() {}
  virtual void OnDragEnd() {}
};

class TrayIcon {
 public:
  virtual ~TrayIcon() = default;

  virtual void SetImage(const gfx::Image& image) = 0;
  virtual void SetPressedImage(const gfx::Image& image) {}
  virtual void SetToolTip(const std::string& tool_tip) = 0;
  virtual void SetTitle(const std::string& title,
                        const std::string& font_type = std::string()) {}
  virtual std::string GetTitle() const { return std::string(); }
  virtual void SetIgnoreDoubleClickEvents(bool ignore) {}
  virtual bool GetIgnoreDoubleClickEvents() const { return false; }
  virtual void SetContextMenu(LynxtronMenuModel* menu) = 0;
  virtual void PopUpContextMenu(LynxtronMenuModel* menu,
                                const TrayPoint& position) = 0;
  virtual void CloseContextMenu() = 0;
  virtual TrayBounds GetBounds() const = 0;
  virtual std::string GetGUID() const = 0;
  virtual void DisplayBalloon(const TrayBalloonOptions& options) {}
  virtual void RemoveBalloon() {}
  virtual void Focus() {}

  static std::unique_ptr<TrayIcon> Create(TrayIconObserver* observer,
                                          const std::string& guid);
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_UI_TRAY_ICON_H_
