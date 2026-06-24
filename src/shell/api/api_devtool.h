// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_API_DEVTOOL_H_
#define LYNXTRON_SHELL_API_API_DEVTOOL_H_

#include <string>

#include "shell/common/gin_helper/arguments.h"
#include "shell/common/gin_helper/handle.h"
#include "shell/common/gin_helper/wrappable.h"

namespace lynxtron {
namespace api {

class Devtool : public gin_helper::DeprecatedWrappable<Devtool> {
 public:
  static Devtool* GetInstance();
  static gin_helper::Handle<Devtool> Create(v8::Isolate* isolate);

  Devtool();
  ~Devtool() override;

  static gin::DeprecatedWrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

 private:
  void SetDevToolEnabled(bool enabled);
  bool IsDevtoolEnabled();
  void SetLogboxEnabled(bool enable);
  bool IsLogboxEnabled();
  void SetOpenCardCallback(gin::Arguments* args);
  void ConnectDevtool(std::string url);
};

}  // namespace api
}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_API_DEVTOOL_H_
