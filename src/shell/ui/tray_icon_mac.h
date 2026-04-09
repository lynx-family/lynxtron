// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_UI_TRAY_ICON_MAC_H_
#define LYNXTRON_SHELL_UI_TRAY_ICON_MAC_H_

#include <string>

#include "tray_icon.h"

namespace lynxtron {

class TrayIconMac : public TrayIcon {
 public:
  TrayIconMac(TrayIconObserver* observer, const std::string& guid);
  ~TrayIconMac() override;

  void SetImage(const gfx::Image& image) override;
  void SetPressedImage(const gfx::Image& image) override;
  void SetToolTip(const std::string& tool_tip) override;
  void SetTitle(const std::string& title) override;
  std::string GetTitle() const override;
  void SetIgnoreDoubleClickEvents(bool ignore) override;
  bool GetIgnoreDoubleClickEvents() const override;
  void SetContextMenu(LynxtronMenuModel* menu) override;
  void PopUpContextMenu(LynxtronMenuModel* menu,
                        const TrayPoint& position) override;
  void CloseContextMenu() override;
  TrayBounds GetBounds() const override;
  std::string GetGUID() const override;

  void HandleAction();
  void HandleMouseEntered(const TrayPoint& position);
  void HandleMouseExited(const TrayPoint& position);
  void HandleMouseMoved(const TrayPoint& position);

 private:
  TrayIconObserver* observer_;
  std::string guid_;
  bool ignore_double_click_events_ = false;
  LynxtronMenuModel* menu_model_ = nullptr;
  void* status_item_ = nullptr;
  void* menu_controller_ = nullptr;
  void* target_ = nullptr;
  void* tracking_area_ = nullptr;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_UI_TRAY_ICON_MAC_H_
