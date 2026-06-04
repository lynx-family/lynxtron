// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_UPDATE_META_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_UPDATE_META_H_

#include <memory>
#include <string>
#include <utility>

namespace lynx {
namespace pub {
class LynxUpdateMeta;
}  // namespace pub
}  // namespace lynx

namespace lynxtron {

class LynxUpdateMeta {
 public:
  LynxUpdateMeta() = default;
  ~LynxUpdateMeta() = default;

  void SetUpdateData(std::string json) { update_data_ = std::move(json); }
  void SetGlobalProps(std::string json) { global_props_ = std::move(json); }

  const std::string& update_data() const { return update_data_; }
  const std::string& global_props() const { return global_props_; }

  std::shared_ptr<lynx::pub::LynxUpdateMeta> BuildCore() const;

 private:
  std::string update_data_;
  std::string global_props_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_LYNX_UPDATE_META_H_
