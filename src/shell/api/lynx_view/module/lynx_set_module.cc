// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/module/lynx_set_module.h"

#include "shell/api/lynx_view/module/lynx_native_module.h"
#include "v8.h"

namespace lynxtron {
namespace {

constexpr char kModuleName[] = "LynxSetModule";

class LynxSetModuleImpl : public LynxNativeModule {
 public:
  LynxSetModuleImpl() {
    RegisterMethod("switchKeyBoardDetect",
                   LynxSetModuleImpl::SwitchKeyBoardDetect);
  }

 private:
  static void SwitchKeyBoardDetect(LynxNativeModule* module,
                                   v8::Isolate* isolate,
                                   v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> const* argv,
                                   int argc) {
    // Keep this method as a no-op on desktop for now.
  }
};

}  // namespace

void RegisterLynxSetModuleToLynxView(lynx_view_builder_t* builder) {
  LynxNativeModule::RegisterLynxNodeModule(builder, kModuleName,
                                           new LynxSetModuleImpl());
}

}  // namespace lynxtron
