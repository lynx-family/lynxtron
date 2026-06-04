// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_API_LYNX_TEMPLATE_BUNDLE_H_
#define LYNXTRON_SHELL_API_API_LYNX_TEMPLATE_BUNDLE_H_
#include <memory>
#include <string>

#include "gin/arguments.h"
#include "shell/common/gin_helper/constructible.h"
#include "shell/common/gin_helper/wrappable.h"
namespace lynx {
namespace pub {
class LynxTemplateBundle;
}  // namespace pub
}  // namespace lynx
namespace gin_helper {
class ErrorThrower;
}
namespace lynxtron::api {
class LynxTemplateBundle final
    : public gin_helper::DeprecatedWrappable<LynxTemplateBundle>,
      public gin_helper::Constructible<LynxTemplateBundle> {
 public:
  static LynxTemplateBundle* New(gin_helper::ErrorThrower thrower,
                                 v8::Local<v8::Value> buffer_like);

  static void FillObjectTemplate(v8::Isolate*, v8::Local<v8::ObjectTemplate>);
  static const char* GetClassName() { return "LynxTemplateBundle"; }

  static gin::DeprecatedWrapperInfo kWrapperInfo;
  const char* GetTypeName() override;

  bool isValid() const;
  std::string getErrorMessage() const;
  std::shared_ptr<lynx::pub::LynxTemplateBundle> GetImpl() const {
    return bundle_;
  }

 private:
  explicit LynxTemplateBundle(
      std::shared_ptr<lynx::pub::LynxTemplateBundle> bundle);
  std::shared_ptr<lynx::pub::LynxTemplateBundle> bundle_;
};
}  // namespace lynxtron::api
#endif  // LYNXTRON_SHELL_API_API_LYNX_TEMPLATE_BUNDLE_H_
