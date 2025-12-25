// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "shell/common/node_includes.h"
#include "shell/lynx_view_holder/lynx_view_extension_register.h"

#define THROW_ERROR_WHEN_FALSE(condition, message)      \
  if (condition != napi_ok) {                           \
    napi_throw_error(env, "Invalid argument", message); \
    return nullptr;                                     \
  }

static napi_value RegisterGlobalEnvModule(napi_env env,
                                          napi_callback_info info) {
  const size_t kExpectedArgc = 4;
  size_t argc = kExpectedArgc;
  napi_value argv[kExpectedArgc];
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok &&
      argc != kExpectedArgc) {
    napi_throw_error(env, "Invalid argument", "Expected 4arguments");
    return nullptr;
  }

  // params
  char name[256];
  void* creator_api = nullptr;
  bool is_lazy_create;
  void* opaque = nullptr;

  THROW_ERROR_WHEN_FALSE(
      napi_get_value_string_utf8(env, argv[0], name, sizeof(name), nullptr),
      "Failed to get name");
  THROW_ERROR_WHEN_FALSE(napi_get_value_external(env, argv[1], &creator_api),
                         "Failed to get creator_api");
  THROW_ERROR_WHEN_FALSE(napi_get_value_bool(env, argv[2], &is_lazy_create),
                         "Failed to get is_lazy_create");
  THROW_ERROR_WHEN_FALSE(napi_get_value_external(env, argv[3], &opaque),
                         "Failed to get opaque");

  lynxtron::RegisterLynxExtensionToGlobalEnv(name, creator_api, is_lazy_create,
                                             opaque);
  return nullptr;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_property_descriptor props[] = {
      {"registerGlobalEnvModule", NULL, RegisterGlobalEnvModule, NULL, NULL,
       NULL, napi_default, NULL},
  };
  status = napi_define_properties(env, exports,
                                  sizeof(props) / sizeof(props[0]), props);
  assert(status == napi_ok);
  return exports;
}

extern "C" {
static napi_module _module = {.nm_version = NAPI_MODULE_VERSION,
                              .nm_flags = 0,
                              .nm_filename = "api_lynx_extension_utils",
                              .nm_register_func = Init,
                              .nm_modname = "lynx_extension",
                              .nm_priv = nullptr,
                              .reserved = {0}};
__attribute__((constructor)) static void register_my_builtin_napi(void) {
  napi_module_register(&_module);
}
};
