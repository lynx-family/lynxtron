// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_API_API_MENU_H_
#define LYNXTRON_SHELL_API_API_MENU_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "shell/api/event_emitter_mixin.h"
#include "shell/common/gin_helper/constructible.h"
#include "shell/common/gin_helper/wrappable.h"
#include "shell/ui/lynxtron_menu_model.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace lynxtron::api {

class BaseWindow;

class Menu : public gin_helper::DeprecatedWrappable<Menu>,
             public gin_helper::EventEmitterMixin<Menu>,
             public gin_helper::Constructible<Menu>,
             public LynxtronMenuModel::Delegate,
             private LynxtronMenuModel::Observer {
 public:
  static Menu* New(gin::Arguments* args);

  explicit Menu(gin::Arguments* args);
  ~Menu() override;

  Menu(const Menu&) = delete;
  Menu& operator=(const Menu&) = delete;

  static gin::DeprecatedWrapperInfo kWrapperInfo;
  const gin::DeprecatedWrapperInfo* wrapper_info() const;
  const char* GetTypeName() override;

  static void FillObjectTemplate(v8::Isolate*, v8::Local<v8::ObjectTemplate>);
  static const char* GetClassName() { return "Menu"; }

#if defined(__APPLE__)
  static void SetApplicationMenu(Menu* menu);
  static void SendActionToFirstResponder(const std::string& action);
#endif

  LynxtronMenuModel* model() const { return model_.get(); }

 protected:
  void RemoveModelObserver();
  base::OnceClosure BindSelfToClosure(base::OnceClosure callback);

  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool ShouldCommandIdWorkWhenHidden(int command_id) const override;
  bool GetAcceleratorForCommandIdWithParams(
      int command_id,
      bool use_default_accelerator,
      ui::Accelerator* accelerator) const override;
  bool ShouldRegisterAcceleratorForCommandId(int command_id) const override;
#if defined(__APPLE__)
  bool GetSharingItemForCommandId(
      int command_id,
      LynxtronMenuModel::SharingItem* item) const override;
  v8::Local<v8::Value> GetUserAcceleratorAt(int command_id) const;
#endif
  void ExecuteCommand(int command_id, int event_flags) override;

  virtual void PopupAt(BaseWindow* window,
                       int x,
                       int y,
                       int positioning_item,
                       base::OnceClosure callback);
  virtual void ClosePopupAt(int32_t window_id);
  virtual std::u16string GetAcceleratorTextAtForTesting(int index) const;

  std::unique_ptr<LynxtronMenuModel> model_;
  raw_ptr<Menu> parent_ = nullptr;

  void OnMenuWillClose() override;
  void OnMenuWillShow() override;

 private:
  void InsertItemAt(int index, int command_id, const std::u16string& label);
  void InsertSeparatorAt(int index);
  void InsertCheckItemAt(int index,
                         int command_id,
                         const std::u16string& label);
  void InsertRadioItemAt(int index,
                         int command_id,
                         const std::u16string& label,
                         int group_id);
  void InsertSubMenuAt(int index,
                       int command_id,
                       const std::u16string& label,
                       Menu* menu);
  void SetIcon(int index, const gfx::Image& image);
  void SetSublabel(int index, const std::u16string& sublabel);
  void SetToolTip(int index, const std::u16string& toolTip);
  void SetRole(int index, const std::u16string& role);
  void SetCustomType(int index, const std::u16string& customType);
  void Clear();
  int GetItemCount() const;
  int GetCommandIdAt(int index) const;
  std::u16string GetLabelAt(int index) const;
  std::u16string GetSublabelAt(int index) const;
  std::u16string GetToolTipAt(int index) const;
  bool IsItemCheckedAt(int index) const;
  bool IsEnabledAt(int index) const;
  bool IsVisibleAt(int index) const;
  bool WorksWhenHiddenAt(int index) const;
};

}  // namespace lynxtron::api

namespace gin {

template <>
struct Converter<lynxtron::LynxtronMenuModel*> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     lynxtron::LynxtronMenuModel** out) {
    if (val->IsNull()) {
      *out = nullptr;
      return true;
    }

    lynxtron::api::Menu* menu;
    if (!Converter<lynxtron::api::Menu*>::FromV8(isolate, val, &menu)) {
      return false;
    }
    *out = menu->model();
    return true;
  }
};

}  // namespace gin

#endif  // LYNXTRON_SHELL_API_API_MENU_H_
