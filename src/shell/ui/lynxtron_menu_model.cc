// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/ui/lynxtron_menu_model.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "accelerator.h"
#include "base/check.h"
#include "build/build_config.h"
#include "gfx/image/image.h"

namespace lynxtron {

#if BUILDFLAG(IS_MAC)
LynxtronMenuModel::SharingItem::SharingItem() = default;
LynxtronMenuModel::SharingItem::SharingItem(SharingItem&&) = default;
LynxtronMenuModel::SharingItem::~SharingItem() = default;
#endif

LynxtronMenuModel::Item::Item(int command_id,
                              ItemType type,
                              std::u16string label)
    : command_id(command_id), type(type), label(std::move(label)) {}

LynxtronMenuModel::Item::Item(Item&&) = default;
LynxtronMenuModel::Item& LynxtronMenuModel::Item::operator=(Item&&) = default;
LynxtronMenuModel::Item::~Item() = default;

LynxtronMenuModel::LynxtronMenuModel(Delegate* delegate)
    : delegate_(delegate) {}

LynxtronMenuModel::~LynxtronMenuModel() = default;

size_t LynxtronMenuModel::GetItemCount() const {
  return items_.size();
}

LynxtronMenuModel::ItemType LynxtronMenuModel::GetTypeAt(size_t index) const {
  return GetItemAt(index)->type;
}

int LynxtronMenuModel::GetCommandIdAt(size_t index) const {
  return GetItemAt(index)->command_id;
}

std::u16string LynxtronMenuModel::GetLabelAt(size_t index) const {
  const Item* item = GetItemAt(index);
  if (delegate_ && item->type != TYPE_SEPARATOR) {
    std::u16string label = delegate_->GetLabelForCommandId(item->command_id);
    if (!label.empty()) {
      return label;
    }
  }
  return item->label;
}

bool LynxtronMenuModel::IsItemCheckedAt(size_t index) const {
  if (!delegate_) {
    return false;
  }
  ItemType type = GetTypeAt(index);
  if (type != TYPE_CHECK && type != TYPE_RADIO) {
    return false;
  }
  return delegate_->IsCommandIdChecked(GetCommandIdAt(index));
}

bool LynxtronMenuModel::IsEnabledAt(size_t index) const {
  if (!delegate_) {
    return true;
  }
  return delegate_->IsCommandIdEnabled(GetCommandIdAt(index));
}

bool LynxtronMenuModel::IsVisibleAt(size_t index) const {
  if (!delegate_) {
    return true;
  }
  return delegate_->IsCommandIdVisible(GetCommandIdAt(index));
}

LynxtronMenuModel* LynxtronMenuModel::GetSubmenuModelAt(size_t index) const {
  return GetItemAt(index)->submenu;
}

void LynxtronMenuModel::SetIcon(size_t index, const gfx::Image& image) {
  GetItemAt(index)->icon = image;
}

gfx::Image LynxtronMenuModel::GetIconAt(size_t index) const {
  const Item* item = GetItemAt(index);
  if (delegate_ && item->type != TYPE_SEPARATOR) {
    gfx::Image icon = delegate_->GetIconForCommandId(item->command_id);
    if (!icon.IsEmpty()) {
      return icon;
    }
  }
  return item->icon;
}

void LynxtronMenuModel::ActivatedAt(size_t index, int event_flags) {
  if (!delegate_) {
    return;
  }
  delegate_->ExecuteCommand(GetItemAt(index)->command_id, event_flags);
}

void LynxtronMenuModel::InsertItemAt(size_t index,
                                     int command_id,
                                     const std::u16string& label) {
  InsertItem(Item(command_id, TYPE_COMMAND, label), index);
}

void LynxtronMenuModel::InsertCheckItemAt(size_t index,
                                          int command_id,
                                          const std::u16string& label) {
  InsertItem(Item(command_id, TYPE_CHECK, label), index);
}

void LynxtronMenuModel::InsertRadioItemAt(size_t index,
                                          int command_id,
                                          const std::u16string& label,
                                          int group_id) {
  Item item(command_id, TYPE_RADIO, label);
  item.group_id = group_id;
  InsertItem(std::move(item), index);
}

void LynxtronMenuModel::InsertSeparatorAt(size_t index) {
  InsertItem(Item(-1, TYPE_SEPARATOR, std::u16string()), index);
}

void LynxtronMenuModel::InsertSubMenuAt(size_t index,
                                        int command_id,
                                        const std::u16string& label,
                                        LynxtronMenuModel* model) {
  Item item(command_id, TYPE_SUBMENU, label);
  item.submenu = model;
  InsertItem(std::move(item), index);
}

void LynxtronMenuModel::Clear() {
  items_.clear();
}

void LynxtronMenuModel::SetToolTip(size_t index,
                                   const std::u16string& toolTip) {
  GetItemAt(index)->tool_tip = toolTip;
}

std::u16string LynxtronMenuModel::GetToolTipAt(size_t index) const {
  return GetItemAt(index)->tool_tip;
}

void LynxtronMenuModel::SetCustomType(size_t index,
                                      const std::u16string& customType) {
  GetItemAt(index)->custom_type = customType;
}

std::u16string LynxtronMenuModel::GetCustomTypeAt(size_t index) const {
  return GetItemAt(index)->custom_type;
}

void LynxtronMenuModel::SetRole(size_t index, const std::u16string& role) {
  GetItemAt(index)->role = role;
}

std::u16string LynxtronMenuModel::GetRoleAt(size_t index) const {
  return GetItemAt(index)->role;
}

void LynxtronMenuModel::SetSecondaryLabel(size_t index,
                                          const std::u16string& sublabel) {
  GetItemAt(index)->secondary_label = sublabel;
}

std::u16string LynxtronMenuModel::GetSecondaryLabelAt(size_t index) const {
  const Item* item = GetItemAt(index);
  if (delegate_ && item->type != TYPE_SEPARATOR) {
    std::u16string label =
        delegate_->GetSecondaryLabelForCommandId(item->command_id);
    if (!label.empty()) {
      return label;
    }
  }
  return item->secondary_label;
}

bool LynxtronMenuModel::GetAcceleratorAtWithParams(
    size_t index,
    bool use_default_accelerator,
    ui::Accelerator* accelerator) const {
  if (delegate_) {
    return delegate_->GetAcceleratorForCommandIdWithParams(
        GetCommandIdAt(index), use_default_accelerator, accelerator);
  }
  return false;
}

bool LynxtronMenuModel::ShouldRegisterAcceleratorAt(size_t index) const {
  if (delegate_) {
    return delegate_->ShouldRegisterAcceleratorForCommandId(
        GetCommandIdAt(index));
  }
  return true;
}

bool LynxtronMenuModel::WorksWhenHiddenAt(size_t index) const {
  if (delegate_) {
    return delegate_->ShouldCommandIdWorkWhenHidden(GetCommandIdAt(index));
  }
  return true;
}

#if BUILDFLAG(IS_MAC)
bool LynxtronMenuModel::GetSharingItemAt(size_t index,
                                         SharingItem* item) const {
  if (delegate_) {
    return delegate_->GetSharingItemForCommandId(GetCommandIdAt(index), item);
  }
  return false;
}

void LynxtronMenuModel::SetSharingItem(SharingItem item) {
  sharing_item_.emplace(std::move(item));
}
#endif

void LynxtronMenuModel::MenuWillClose() {
  observers_.Notify(&Observer::OnMenuWillClose);
}

void LynxtronMenuModel::MenuWillShow() {
  observers_.Notify(&Observer::OnMenuWillShow);
}

void LynxtronMenuModel::InsertItem(Item item, size_t index) {
  if (index > items_.size()) {
    index = items_.size();
  }
  items_.insert(items_.begin() + static_cast<ptrdiff_t>(index),
                std::move(item));
}

const LynxtronMenuModel::Item* LynxtronMenuModel::GetItemAt(
    size_t index) const {
  CHECK_LT(index, items_.size());
  return &items_[index];
}

LynxtronMenuModel::Item* LynxtronMenuModel::GetItemAt(size_t index) {
  CHECK_LT(index, items_.size());
  return &items_[index];
}

std::optional<size_t> LynxtronMenuModel::GetIndexOfCommandId(
    int command_id) const {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].command_id == command_id) {
      return i;
    }
  }
  return std::nullopt;
}

}  // namespace lynxtron
