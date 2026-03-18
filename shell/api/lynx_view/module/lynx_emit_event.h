// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_EMIT_EVENT_H_
#define LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_EMIT_EVENT_H_

#include "shell/common/gin_helper/handle.h"
#include "shell/common/gin_helper/wrappable.h"

namespace gin_helper {
template <typename T>
class Handle;
}  // namespace gin_helper

namespace v8 {
class Isolate;
template <typename T>
class Local;
class Object;
class ObjectTemplate;
}  // namespace v8

namespace lynxtron {

class LynxEmitEvent : public gin_helper::DeprecatedWrappable<LynxEmitEvent> {
  using InvokeCallback =
      std::function<void(v8::Isolate* isolate, v8::Local<v8::Value> arg)>;

 public:
  static gin_helper::Handle<LynxEmitEvent> Create(v8::Isolate* isolate,
                                                  InvokeCallback callback);

  // gin_helper::Wrappable
  static gin::DeprecatedWrapperInfo kWrapperInfo;
  const char* GetTypeName() override { return "LynxEmitEvent"; }
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

 private:
  explicit LynxEmitEvent(InvokeCallback callback);
  ~LynxEmitEvent() override;

  bool SendReply(v8::Isolate* isolate, v8::Local<v8::Value> arg);

  InvokeCallback callback_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_LYNX_VIEW_MODULE_LYNX_EMIT_EVENT_H_
