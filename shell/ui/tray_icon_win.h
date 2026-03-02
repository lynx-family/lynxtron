// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_UI_TRAY_ICON_WIN_H_
#define LYNXTRON_SHELL_UI_TRAY_ICON_WIN_H_

#include <string>

#include "build/build_config.h"
#include "tray_icon.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace lynxtron {

#if BUILDFLAG(IS_WIN)

class TrayIconWin : public TrayIcon {
 public:
  TrayIconWin(TrayIconObserver* observer, const std::string& guid);
  ~TrayIconWin() override;

  void SetImage(const gfx::Image& image) override;
  void SetToolTip(const std::string& tool_tip) override;
  void SetContextMenu(LynxtronMenuModel* menu) override;
  void PopUpContextMenu(LynxtronMenuModel* menu,
                        const TrayPoint& position) override;
  void CloseContextMenu() override;
  TrayBounds GetBounds() const override;
  std::string GetGUID() const override;
  void DisplayBalloon(const TrayBalloonOptions& options) override;
  void RemoveBalloon() override;
  void Focus() override;

  void ResetIcon();

  void HandleTrayMessage(UINT message, WPARAM w_param, LPARAM l_param);

 private:
  TrayIconObserver* observer_;
  std::string guid_;
  LynxtronMenuModel* menu_model_ = nullptr;
  bool added_ = false;
  bool hovering_ = false;
  UINT icon_id_ = 0;
  HWND window_ = nullptr;
  HICON icon_ = nullptr;

  void AddIcon();
  void UpdateIcon();
  void DeleteIcon();
  void ShowContextMenu(const TrayPoint& position);
};

#endif

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_UI_TRAY_ICON_WIN_H_
