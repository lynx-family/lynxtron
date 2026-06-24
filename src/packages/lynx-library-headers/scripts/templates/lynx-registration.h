#ifndef LYNX_EXTENSION_HEADERS_LYNX_REGISTRATION_H_
#define LYNX_EXTENSION_HEADERS_LYNX_REGISTRATION_H_

#include <node_api.h>

#if __has_include(<lynx_extension.h>)
#include <lynx_extension.h>
#elif __has_include(<lynx/extension.h>)
#include <lynx/extension.h>
#endif

#include <stdint.h>

namespace lynx {
namespace registration {

#if defined(USE_WEAK_SUFFIX_NAPI)
#include "third_party/weak-node-api/headers/weak_napi_defines.h"
using LynxNapiEnv = napi_env;
using LynxNapiValue = napi_value;
using LynxNativeModuleCreator = napi_module_creator;
#else
using LynxNapiEnv = napi_env;
using LynxNapiValue = napi_value;
using LynxNativeModuleCreator = napi_module_creator;
#endif

struct ElementRegistration {
  const char* module_name;
  const char* element_name;
  lynx_native_view_creator create_native_view;
  bool is_lazy_create;
  void* opaque;
};

inline void Check(napi_env env, napi_status status) {
  if (status != napi_ok) {
    napi_throw_error(env, nullptr, "N-API call failed");
  }
}

inline void OnRuntimeInit(lynx_extension_module_t* module) {
  (void)module;
}

inline void OnRuntimeAttach(lynx_extension_module_t* module,
                            napi_env env,
                            lynx_vsync_observer_t* vsync_observer) {
  (void)module;
  (void)env;
  (void)vsync_observer;
}

inline void RegisterNativeView(lynx_extension_module_t* module,
                               lynx_view_t* lynx_view) {
  if (module == nullptr || lynx_view == nullptr) {
    return;
  }

  auto* registration = static_cast<ElementRegistration*>(
      lynx_extension_module_get_user_data(module));

  if (registration != nullptr && registration->element_name != nullptr &&
      registration->create_native_view != nullptr) {
    // Keep explicit opaque values, otherwise pass the current LynxView so
    // native UI implementations can resolve their platform parent window.
    void* native_view_opaque =
        registration->opaque != nullptr ? registration->opaque : lynx_view;
    lynx_view_register_native_view(lynx_view, registration->element_name,
                                   registration->create_native_view,
                                   native_view_opaque);
  }
}

inline lynx_extension_module_t* CreateElementModule(void* opaque) {
  auto* registration = static_cast<ElementRegistration*>(opaque);
  lynx_extension_module_t* module = lynx_extension_module_create(registration);
  lynx_extension_module_bind_runtime_init(module, OnRuntimeInit);
  lynx_extension_module_bind_runtime_attach(module, OnRuntimeAttach);
  lynx_extension_module_bind_lynx_view_create(module, RegisterNativeView);
  return module;
}

inline void RegisterNativeModule(const char* name,
                                 LynxNativeModuleCreator creator,
                                 void* opaque) {
  if (name == nullptr || creator == nullptr) {
    return;
  }
  lynx_env_register_native_module(name, creator, opaque);
}

inline void RegisterElement(ElementRegistration* registration) {
  if (registration == nullptr || registration->module_name == nullptr ||
      registration->element_name == nullptr ||
      registration->create_native_view == nullptr) {
    return;
  }
  lynx_env_register_extension_module(
      registration->module_name, CreateElementModule,
      registration->is_lazy_create, registration);
}

}  // namespace registration
}  // namespace lynx

#if defined(_MSC_VER)
#define LYNX_REGISTRATION_CONSTRUCTOR(NAME) \
  static void NAME(void);                   \
  namespace {                               \
  struct NAME##Constructor {                \
    NAME##Constructor() {                   \
      NAME();                               \
    }                                       \
  } NAME##ConstructorInstance;              \
  }                                         \
  static void NAME(void)
#else
#define LYNX_REGISTRATION_CONSTRUCTOR(NAME) \
  __attribute__((constructor)) static void NAME(void)
#endif

#define LYNX_REGISTRATION_CONCAT_INNER(A, B) A##B
#define LYNX_REGISTRATION_CONCAT(A, B) LYNX_REGISTRATION_CONCAT_INNER(A, B)

#define LYNX_REGISTER_NATIVE_MODULE(NAME, CREATE_NATIVE_MODULE, OPAQUE)       \
  LYNX_REGISTRATION_REGISTER_NATIVE_MODULE_UNIQUE(NAME, CREATE_NATIVE_MODULE, \
                                                  OPAQUE, __COUNTER__)

#define LYNX_REGISTRATION_REGISTER_NATIVE_MODULE_UNIQUE( \
    NAME, CREATE_NATIVE_MODULE, OPAQUE, ID)              \
  LYNX_REGISTRATION_REGISTER_NATIVE_MODULE_DISPATCH(     \
      NAME, CREATE_NATIVE_MODULE, OPAQUE,                \
      LYNX_REGISTRATION_CONCAT(LynxNativeModule, ID))

#define LYNX_REGISTRATION_REGISTER_NATIVE_MODULE_DISPATCH(                  \
    NAME, CREATE_NATIVE_MODULE, OPAQUE, ID)                                 \
  LYNX_REGISTRATION_REGISTER_NATIVE_MODULE_IMPL(NAME, CREATE_NATIVE_MODULE, \
                                                OPAQUE, ID)

#define LYNX_REGISTRATION_REGISTER_NATIVE_MODULE_IMPL(                     \
    NAME, CREATE_NATIVE_MODULE, OPAQUE, ID)                                \
  namespace {                                                              \
  LYNX_REGISTRATION_CONSTRUCTOR(ID##Register) {                            \
    ::lynx::registration::RegisterNativeModule(NAME, CREATE_NATIVE_MODULE, \
                                               OPAQUE);                    \
  }                                                                        \
  }

#define LYNX_REGISTER_ELEMENT(MODULE_NAME, ELEMENT_NAME, CREATE_NATIVE_VIEW, \
                              IS_LAZY_CREATE, OPAQUE)                        \
  LYNX_REGISTRATION_REGISTER_ELEMENT_UNIQUE(                                 \
      MODULE_NAME, ELEMENT_NAME, CREATE_NATIVE_VIEW, IS_LAZY_CREATE, OPAQUE, \
      __COUNTER__)

#define LYNX_REGISTRATION_REGISTER_ELEMENT_UNIQUE(                             \
    MODULE_NAME, ELEMENT_NAME, CREATE_NATIVE_VIEW, IS_LAZY_CREATE, OPAQUE, ID) \
  LYNX_REGISTRATION_REGISTER_ELEMENT_DISPATCH(                                 \
      MODULE_NAME, ELEMENT_NAME, CREATE_NATIVE_VIEW, IS_LAZY_CREATE, OPAQUE,   \
      LYNX_REGISTRATION_CONCAT(LynxElementModule, ID))

#define LYNX_REGISTRATION_REGISTER_ELEMENT_DISPATCH(                           \
    MODULE_NAME, ELEMENT_NAME, CREATE_NATIVE_VIEW, IS_LAZY_CREATE, OPAQUE, ID) \
  LYNX_REGISTRATION_REGISTER_ELEMENT_IMPL(MODULE_NAME, ELEMENT_NAME,           \
                                          CREATE_NATIVE_VIEW, IS_LAZY_CREATE,  \
                                          OPAQUE, ID)

#define LYNX_REGISTRATION_REGISTER_ELEMENT_IMPL(                               \
    MODULE_NAME, ELEMENT_NAME, CREATE_NATIVE_VIEW, IS_LAZY_CREATE, OPAQUE, ID) \
  namespace {                                                                  \
  ::lynx::registration::ElementRegistration ID##Registration = {               \
      MODULE_NAME, ELEMENT_NAME, CREATE_NATIVE_VIEW, IS_LAZY_CREATE, OPAQUE};  \
  LYNX_REGISTRATION_CONSTRUCTOR(ID##Register) {                                \
    ::lynx::registration::RegisterElement(&ID##Registration);                  \
  }                                                                            \
  }

#endif  // LYNX_EXTENSION_HEADERS_LYNX_REGISTRATION_H_
