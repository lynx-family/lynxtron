// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_UI_LYNXTRON_MENU_MODEL_H_
#define LYNXTRON_SHELL_UI_LYNXTRON_MENU_MODEL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "shell/ui/accelerator.h"
#include "shell/ui/gfx/image/image.h"
#include "url/gurl.h"

namespace lynxtron {

class LynxtronMenuModel {
 public:
  enum ItemType {
    TYPE_COMMAND,
    TYPE_CHECK,
    TYPE_RADIO,
    TYPE_SEPARATOR,
    TYPE_SUBMENU,
  };
#if BUILDFLAG(IS_MAC)
  struct SharingItem {
    SharingItem();
    SharingItem(SharingItem&&);
    SharingItem(const SharingItem&) = delete;
    ~SharingItem();

    std::optional<std::vector<std::string>> texts;
    std::optional<std::vector<GURL>> urls;
    std::optional<std::vector<base::FilePath>> file_paths;
  };
#endif

  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual bool IsCommandIdChecked(int command_id) const = 0;
    virtual bool IsCommandIdEnabled(int command_id) const = 0;
    virtual bool IsCommandIdVisible(int command_id) const = 0;
    virtual std::u16string GetLabelForCommandId(int command_id) const = 0;
    virtual std::u16string GetSecondaryLabelForCommandId(
        int command_id) const = 0;
    virtual gfx::Image GetIconForCommandId(int command_id) const = 0;
    virtual void ExecuteCommand(int command_id, int event_flags) = 0;

    virtual bool GetAcceleratorForCommandIdWithParams(
        int command_id,
        bool use_default_accelerator,
        ui::Accelerator* accelerator) const = 0;

    virtual bool ShouldRegisterAcceleratorForCommandId(
        int command_id) const = 0;

    virtual bool ShouldCommandIdWorkWhenHidden(int command_id) const = 0;

#if BUILDFLAG(IS_MAC)
    virtual bool GetSharingItemForCommandId(int command_id,
                                            SharingItem* item) const = 0;
#endif
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnMenuWillShow() {}
    virtual void OnMenuWillClose() {}
  };

  explicit LynxtronMenuModel(Delegate* delegate);
  ~LynxtronMenuModel();

  LynxtronMenuModel(const LynxtronMenuModel&) = delete;
  LynxtronMenuModel& operator=(const LynxtronMenuModel&) = delete;

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  void InsertItemAt(size_t index, int command_id, const std::u16string& label);
  void InsertCheckItemAt(size_t index,
                         int command_id,
                         const std::u16string& label);
  void InsertRadioItemAt(size_t index,
                         int command_id,
                         const std::u16string& label,
                         int group_id);
  void InsertSeparatorAt(size_t index);
  void InsertSubMenuAt(size_t index,
                       int command_id,
                       const std::u16string& label,
                       LynxtronMenuModel* model);

  void Clear();

  size_t GetItemCount() const;
  ItemType GetTypeAt(size_t index) const;
  int GetCommandIdAt(size_t index) const;
  std::optional<size_t> GetIndexOfCommandId(int command_id) const;
  std::u16string GetLabelAt(size_t index) const;
  bool IsItemCheckedAt(size_t index) const;
  bool IsEnabledAt(size_t index) const;
  bool IsVisibleAt(size_t index) const;
  LynxtronMenuModel* GetSubmenuModelAt(size_t index) const;
  void SetIcon(size_t index, const gfx::Image& image);
  gfx::Image GetIconAt(size_t index) const;
  void ActivatedAt(size_t index, int event_flags);

  void SetToolTip(size_t index, const std::u16string& toolTip);
  std::u16string GetToolTipAt(size_t index) const;
  void SetCustomType(size_t index, const std::u16string& customType);
  std::u16string GetCustomTypeAt(size_t index) const;
  void SetRole(size_t index, const std::u16string& role);
  std::u16string GetRoleAt(size_t index) const;
  void SetSecondaryLabel(size_t index, const std::u16string& sublabel);
  std::u16string GetSecondaryLabelAt(size_t index) const;
  bool GetAcceleratorAtWithParams(size_t index,
                                  bool use_default_accelerator,
                                  ui::Accelerator* accelerator) const;
  bool ShouldRegisterAcceleratorAt(size_t index) const;
  bool WorksWhenHiddenAt(size_t index) const;
#if BUILDFLAG(IS_MAC)
  bool GetSharingItemAt(size_t index, SharingItem* item) const;
  void SetSharingItem(SharingItem item);
  [[nodiscard]] const std::optional<SharingItem>& sharing_item() const {
    return sharing_item_;
  }
#endif

  void MenuWillClose();
  void MenuWillShow();

  base::WeakPtr<LynxtronMenuModel> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  struct Item {
    Item(int command_id, ItemType type, std::u16string label);
    Item(Item&&);
    Item& operator=(Item&&);
    ~Item();

    int command_id = 0;
    ItemType type = TYPE_COMMAND;
    std::u16string label;
    std::u16string tool_tip;
    std::u16string role;
    std::u16string secondary_label;
    std::u16string custom_type;
    gfx::Image icon;
    int group_id = -1;
    raw_ptr<LynxtronMenuModel> submenu = nullptr;
    bool enabled = true;
    bool visible = true;
  };

  void InsertItem(Item item, size_t index);
  const Item* GetItemAt(size_t index) const;
  Item* GetItemAt(size_t index);

  raw_ptr<Delegate> delegate_;

#if BUILDFLAG(IS_MAC)
  std::optional<SharingItem> sharing_item_;
#endif

  std::vector<Item> items_;
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<LynxtronMenuModel> weak_factory_{this};
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_UI_LYNXTRON_MENU_MODEL_H_
