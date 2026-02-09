// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_API_MENU_MAC_H_
#define LYNXTRON_SHELL_API_API_MENU_MAC_H_

#include <map>

#include "shell/api/api_menu.h"
#import "shell/ui/cocoa/lynxtron_menu_controller.h"

namespace lynxtron {
class NativeWindow;

namespace api {

class MenuMac : public Menu {
 public:
  explicit MenuMac(gin::Arguments* args);
  ~MenuMac() override;

 protected:
  void PopupAt(BaseWindow* window,
               int x,
               int y,
               int positioning_item,
               base::OnceClosure callback) override;
  void PopupOnUI(const base::WeakPtr<NativeWindow>& native_window,
                 int32_t window_id,
                 int x,
                 int y,
                 int positioning_item,
                 base::OnceClosure callback);
  void ClosePopupAt(int32_t window_id) override;
  std::u16string GetAcceleratorTextAtForTesting(int index) const override;

 private:
  friend class Menu;

  void ClosePopupOnUI(int32_t window_id);
  void OnClosed(int32_t window_id, base::OnceClosure callback);

  LynxtronMenuController* __strong menu_controller_;
  std::map<int32_t, LynxtronMenuController*> popup_controllers_;

  base::WeakPtrFactory<MenuMac> weak_factory_{this};
};

}  // namespace api
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_API_MENU_MAC_H_
