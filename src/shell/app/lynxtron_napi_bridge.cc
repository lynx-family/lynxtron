// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Small OHOS NAPI bridge that ETS imports as `liblynxtron_napi.so`.
//
// This .so is intentionally tiny and does NOT link to lynxtron_lib (which
// would drag in Node.js and pollute the export table with internal napi_*
// symbols, confusing the OHOS NAPI framework). It uses the OHOS system
// NAPI (libace_napi.z.so) cleanly via the standard <napi/native_api.h>
// header from the OHOS NDK sysroot.
//
// Mirrors chromium114-electron/src/ohos/adapter/native_initializer.cc
// which builds libadapter.so as the ETS-facing bridge while the big
// libelectron.so stays out of the import path.

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <arkui/ui_input_event.h>
#include <dlfcn.h>
#include <hilog/log.h>

#include "shell/app/lynx_windowless_renderer_harmony.h"
#include "shell/app/window_creation_bridge_harmony.h"
#include <inputmethod/inputmethod_attach_options_capi.h>
#include <inputmethod/inputmethod_controller_capi.h>
#include <inputmethod/inputmethod_cursor_info_capi.h>
#include <inputmethod/inputmethod_inputmethod_proxy_capi.h>
#include <inputmethod/inputmethod_text_avoid_info_capi.h>
#include <inputmethod/inputmethod_text_config_capi.h>
#include <inputmethod/inputmethod_text_editor_proxy_capi.h>
#include <napi/native_api.h>
#include <stdlib.h>
#include <unistd.h>
#include <window_manager/oh_display_manager.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "LynxtronBridge"

namespace {

// Plain C enum matching gfx::ResizeEdge in
// shell/ui/gfx/geometry/resize_utils.h. The NAPI bridge and liblynxtron.so do
// not share C++ headers, so the values are duplicated here on purpose.
enum LynxtronResizeEdge {
  kLynxtronResizeEdgeBottom = 0,
  kLynxtronResizeEdgeBottomLeft = 1,
  kLynxtronResizeEdgeBottomRight = 2,
  kLynxtronResizeEdgeLeft = 3,
  kLynxtronResizeEdgeRight = 4,
  kLynxtronResizeEdgeTop = 5,
  kLynxtronResizeEdgeTopLeft = 6,
  kLynxtronResizeEdgeTopRight = 7,
};

using LynxtronMainFn = int (*)(int, char**);
using LynxtronSetWindowIdFn = void (*)(int32_t);
using LynxtronRegisterWindowOpCallbackFn =
    void (*)(int32_t, void (*)(int32_t, const char*, const char*));
using LynxtronGetWindowIdFn = int32_t (*)();
using LynxtronNotifyWindowStateFn =
    void (*)(int32_t, const char*, int32_t);
using LynxtronNotifyWindowRectFn =
    void (*)(int32_t, int32_t, int32_t, int32_t, int32_t);
using LynxtronHandleOpenURLFn = void (*)(const char*);
using LynxtronHandleOpenPathFn = void (*)(const char*);
using LynxtronQuitFn = void (*)();

void* g_lynxtron_handle = nullptr;
LynxtronMainFn g_lynxtron_main = nullptr;
LynxtronHandleOpenURLFn g_handle_open_url = nullptr;
LynxtronHandleOpenPathFn g_handle_open_path = nullptr;
LynxtronQuitFn g_quit = nullptr;

// Thread-safe bridge from the detached LynxtronMain thread back to the ArkUI
// main thread. When LynxtronMain returns, we must NOT call exit()/_exit()
// (appspawn_server aborts the process with SIGABRT on that), so we post the
// exit code through this; the ArkUI thread then calls terminateSelf() on the
// stored ability context (see ExitCallJS). Event-driven, no polling.
napi_threadsafe_function g_exit_tsfn = nullptr;

// Logs the default display's available area at startup; diagnostic for
// window sizing and centering.
void LogDefaultDisplayAvailableArea() {
  uint64_t display_id = 0;
  NativeDisplayManager_ErrorCode status =
      OH_NativeDisplayManager_GetDefaultDisplayId(&display_id);
  if (status != DISPLAY_MANAGER_OK) {
    OH_LOG_ERROR(LOG_APP, "GetDefaultDisplayId failed: %{public}d",
                 static_cast<int>(status));
    return;
  }

  NativeDisplayManager_Rect* available_area = nullptr;
  status = OH_NativeDisplayManager_CreateAvailableArea(display_id,
                                                        &available_area);
  if (status != DISPLAY_MANAGER_OK || !available_area) {
    OH_LOG_ERROR(LOG_APP,
                 "CreateAvailableArea failed: display=%{public}llu "
                 "status=%{public}d",
                 static_cast<unsigned long long>(display_id),
                 static_cast<int>(status));
    return;
  }

  OH_LOG_INFO(LOG_APP,
              "AvailableArea: display=%{public}llu left=%{public}d "
              "top=%{public}d width=%{public}u height=%{public}u",
              static_cast<unsigned long long>(display_id),
              available_area->left, available_area->top, available_area->width,
              available_area->height);
  OH_NativeDisplayManager_DestroyAvailableArea(available_area);
}

// Returns the id of the internal/main display. In mirror mode the system's
// "default" display may be the external/mirrored display, which cannot host
// independent Ability windows. Always launching on the internal display avoids
// creating a WindowAbility that never reaches onWindowStageCreate.
uint64_t GetInternalDisplayIdNative() {
  uint64_t fallback_id = 0;
  OH_NativeDisplayManager_GetDefaultDisplayId(&fallback_id);

  NativeDisplayManager_DisplaysInfo* info = nullptr;
  NativeDisplayManager_ErrorCode status =
      OH_NativeDisplayManager_CreateAllDisplays(&info);
  if (status != DISPLAY_MANAGER_OK || !info) {
    OH_LOG_WARN(LOG_APP,
                "GetInternalDisplayIdNative: CreateAllDisplays failed ret=%{public}d",
                static_cast<int>(status));
    return fallback_id;
  }

  uint64_t internal_id = fallback_id;
  for (uint32_t i = 0; i < info->displaysLength; ++i) {
    const auto* di = &info->displaysInfo[i];
    NativeDisplayManager_SourceMode mode;
    if (OH_NativeDisplayManager_GetDisplaySourceMode(di->id, &mode) ==
            DISPLAY_MANAGER_OK &&
        mode == DISPLAY_SOURCE_MODE_MAIN) {
      internal_id = di->id;
      OH_LOG_INFO(LOG_APP,
                  "GetInternalDisplayIdNative: internal display id=%{public}llu",
                  static_cast<unsigned long long>(internal_id));
      break;
    }
  }

  OH_NativeDisplayManager_DestroyAllDisplays(info);
  return internal_id;
}

napi_value GetInternalDisplayId(napi_env env, napi_callback_info) {
  napi_value result = nullptr;
  napi_create_int64(env, static_cast<int64_t>(GetInternalDisplayIdNative()),
                    &result);
  return result;
}

bool EnsureLynxtronLoaded() {
  if (g_lynxtron_main) return true;

  // Try RTLD_NOLOAD first — if the system already loaded liblynxtron.so
  // (because it's in libs/arm64-v8a/), this returns its handle without
  // running static initializers, avoiding partition_alloc dlopen crashes.
  OH_LOG_INFO(LOG_APP, "dlopen liblynxtron.so (NOLOAD)...");
  g_lynxtron_handle = dlopen("liblynxtron.so", RTLD_NOLOAD);
  if (!g_lynxtron_handle) {
    OH_LOG_INFO(LOG_APP, "Not preloaded, trying RTLD_NOW...");
    g_lynxtron_handle = dlopen("liblynxtron.so", RTLD_NOW);
  }
  if (!g_lynxtron_handle) {
    OH_LOG_ERROR(LOG_APP, "dlopen liblynxtron.so FAILED: %{public}s",
                 dlerror());
    return false;
  }
  OH_LOG_INFO(LOG_APP, "dlopen liblynxtron.so OK: %{public}p",
              g_lynxtron_handle);

  g_lynxtron_main = reinterpret_cast<LynxtronMainFn>(
      dlsym(g_lynxtron_handle, "LynxtronMain"));
  if (!g_lynxtron_main) {
    OH_LOG_ERROR(LOG_APP, "dlsym LynxtronMain FAILED: %{public}s",
                 dlerror());
    return false;
  }
  OH_LOG_INFO(LOG_APP, "LynxtronMain symbol @ %{public}p",
              (void*)g_lynxtron_main);
  return true;
}

// Defined further down; registers WindowCommandFromNative into liblynxtron.so.
void RegisterWindowCommandHandler();

// Defined further down; invoked by liblynxtron.so when a window's editable
// focus or caret changes.
void OnHarmonyTextInputFocus(int32_t harmony_window_id,
                             bool visible,
                             float x,
                             float y,
                             float width,
                             float height);

napi_value Start(napi_env env, napi_callback_info info) {
  OH_LOG_INFO(LOG_APP, "Start() called from ETS");
  LogDefaultDisplayAvailableArea();
  // Mirrors electron_main_ohos.cc: Node.js module resolution needs NODE_PATH
  // pointing at the HAP libs directory (where bundled .so / asar deps live).
  setenv("NODE_PATH",
         "/data/storage/el1/bundle/libs/arm64/node_modules.asar.unpacked/:"
         "/data/storage/el1/bundle/libs/arm64/",
         /*overwrite=*/1);
  OH_LOG_INFO(LOG_APP, "NODE_PATH set");

  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "Failed to load liblynxtron.so");
    return nullptr;
  }

  // The main library is now loaded; wire up window-decor dispatch so
  // setWindowButtonVisibility flows from NativeWindowHarmony to the OHOS
  // window.Window.
  RegisterWindowCommandHandler();

  // Wire up per-window text-input focus/caret events so IME follows the actual
  // editable focus instead of the window-creation-time window id.
  using SetTextInputFocusCallbackFn =
      void (*)(lynxtron::HarmonyTextInputFocusCallback);
  auto set_focus_callback = reinterpret_cast<SetTextInputFocusCallbackFn>(
      dlsym(g_lynxtron_handle, "LynxtronSetHarmonyTextInputFocusCallback"));
  if (set_focus_callback) {
    set_focus_callback(&OnHarmonyTextInputFocus);
    OH_LOG_INFO(LOG_APP, "[IME] text-input focus callback registered");
  } else {
    OH_LOG_ERROR(LOG_APP,
                 "[IME] dlsym LynxtronSetHarmonyTextInputFocusCallback FAILED: "
                 "%{public}s",
                 dlerror());
  }

  // LynxtronMain runs chromium's blocking run_loop->Run(). The OHOS UIAbility
  // main (ETS) thread must keep pumping the ArkUI event loop, so running
  // LynxtronMain here would trigger THREAD_BLOCK watchdog (6s) and force-exit.
  // Spawn it on a dedicated thread (detached) so both loops coexist.
  static std::once_flag started;
  std::call_once(started, [] {
    std::thread([] {
      OH_LOG_INFO(LOG_APP, "LynxtronMain thread start...");
      static char argv0[] = "lynxtron";
      std::string enable_logging = "--enable-logging";
      std::string log_file_switch =
          "--log-file=/data/storage/el2/base/haps/entry/files/"
          "lynxtron_debug.log";
      char* argv[] = {argv0, enable_logging.data(), log_file_switch.data(),
                      nullptr};
      OH_LOG_INFO(LOG_APP, "file logging enabled: %{public}s",
                  "/data/storage/el2/base/haps/entry/files/"
                  "lynxtron_debug.log");
      int rc = g_lynxtron_main(3, argv);
      OH_LOG_INFO(LOG_APP, "LynxtronMain returned rc=%{public}d", rc);
      // Do NOT exit()/_exit() here — appspawn_server aborts on that. Post the
      // exit code to the ArkUI main thread through the thread-safe function;
      // ExitCallJS runs there, frees the heap-allocated code, and calls
      // terminateSelf() on the ability context.
      if (g_exit_tsfn) {
        int* prc = new int(rc);
        napi_status s = napi_call_threadsafe_function(g_exit_tsfn, prc,
                                                       napi_tsfn_nonblocking);
        if (s != napi_ok) {
          delete prc;
          OH_LOG_ERROR(LOG_APP, "napi_call_threadsafe_function failed: %{public}d",
                       static_cast<int>(s));
        }
        napi_release_threadsafe_function(g_exit_tsfn, napi_tsfn_release);
        g_exit_tsfn = nullptr;
      } else {
        OH_LOG_ERROR(LOG_APP, "exit tsfn missing — cannot terminate ability");
      }
    }).detach();
  });

  napi_value result = nullptr;
  napi_create_int32(env, 0, &result);
  return result;
}

// Mirrors electron_ohos's kAppQuit command: the ArkTS/Ability side asks the
// browser to exit gracefully (Application::Quit) instead of force-killing the
// process. After the graceful quit completes, LynxtronMain returns, the exit
// code is posted through the thread-safe function and ArkUI terminates the
// ability.
napi_value Quit(napi_env env, napi_callback_info) {
  if (!g_quit) {
    EnsureLynxtronLoaded();
    if (g_lynxtron_handle) {
      g_quit = reinterpret_cast<LynxtronQuitFn>(
          dlsym(g_lynxtron_handle, "LynxtronQuit"));
    }
  }
  if (g_quit) {
    g_quit();
    OH_LOG_INFO(LOG_APP, "quit() dispatched to LynxtronQuit");
  } else {
    OH_LOG_WARN(LOG_APP, "LynxtronQuit symbol not found");
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

// ---------------------------------------------------------------------------
// Window operation bridge
//
// ETS registers a single callback that receives window operation requests
// (e.g. "minimize", "hide", "setAlwaysOnTop") from liblynxtron's main thread.
// We forward them through a napi_threadsafe_function so the JS callback runs
// on the ArkUI thread where the Window object is valid.
// ---------------------------------------------------------------------------

struct WindowOpData {
  int32_t windowId;
  std::string op;
  std::string args;
};

std::unordered_map<int32_t, napi_threadsafe_function> g_window_op_tsfn_map;
std::mutex g_window_op_tsfn_mutex;

// TSFN callback running on the ArkUI thread: invokes the registered ArkTS
// window-op handler with (windowId, op, args).
void WindowOpCallJS(napi_env env, napi_value js_cb, void* context,
                    void* data) {
  if (!env || !js_cb || !data) return;
  std::unique_ptr<WindowOpData> op(static_cast<WindowOpData*>(data));

  napi_value argv[3];
  napi_create_int32(env, op->windowId, &argv[0]);
  napi_create_string_utf8(env, op->op.c_str(), op->op.length(), &argv[1]);
  napi_create_string_utf8(env, op->args.c_str(), op->args.length(), &argv[2]);

  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, js_cb, 3, argv, nullptr);
}

void WindowOpNativeCallback(int32_t window_id, const char* op, const char* args) {
  if (!op) {
    OH_LOG_WARN(LOG_APP, "[WindowOp] no op, dropping for id=%{public}d",
                window_id);
    return;
  }
  std::lock_guard<std::mutex> lock(g_window_op_tsfn_mutex);
  auto it = g_window_op_tsfn_map.find(window_id);
  if (it == g_window_op_tsfn_map.end() || !it->second) {
    OH_LOG_WARN(LOG_APP,
                "[WindowOp] no tsfn for id=%{public}d, dropping %{public}s",
                window_id, op);
    return;
  }
  auto data = std::make_unique<WindowOpData>(
      WindowOpData{window_id, std::string(op), args ? args : ""});
  napi_status status = napi_call_threadsafe_function(
      it->second, data.release(), napi_tsfn_nonblocking);
  OH_LOG_INFO(LOG_APP,
              "[WindowOp] posted id=%{public}d %{public}s args=%{public}s status=%{public}d",
              window_id, op, args ? args : "(null)", (int)status);
}

// ---------------------------------------------------------------------------
// Window creation bridge
//
// liblynxtron's NativeWindowHarmony requests a new HarmonyOS window by calling
// LynxtronCreateHarmonyWindow. We forward the request to the ArkUI thread via
// a threadsafe function registered by AppWindowAdapter.createWindowFromCpp.
// The ArkTS side creates the Ability/Window and invokes our completion
// callback, which routes back to liblynxtron via LynxtronOnHarmonyWindowCreated.
// ---------------------------------------------------------------------------

struct CreateWindowRequest {
  int32_t window_id;
  lynxtron::HarmonyWindowCreationOptions options;
};

napi_threadsafe_function g_create_window_tsfn = nullptr;
std::mutex g_create_window_mutex;

// TSFN callback running on the ArkUI thread: builds the CreateWindowOptions
// object from the request and invokes the ArkTS create-window handler.
void CreateWindowCallJS(napi_env env, napi_value js_cb, void* context,
                        void* data) {
  if (!env || !js_cb || !data) return;
  std::unique_ptr<CreateWindowRequest> request(
      static_cast<CreateWindowRequest*>(data));

  // Build the options object that matches ArkTS CreateWindowOptions.
  napi_value options;
  napi_create_object(env, &options);

  auto set_int = [&](const char* key, int32_t value) {
    napi_value v;
    napi_create_int32(env, value, &v);
    napi_set_named_property(env, options, key, v);
  };
  auto set_bool = [&](const char* key, bool value) {
    napi_value v;
    napi_get_boolean(env, value, &v);
    napi_set_named_property(env, options, key, v);
  };
  auto set_string = [&](const char* key, const std::string& value) {
    napi_value v;
    napi_create_string_utf8(env, value.c_str(), value.length(), &v);
    napi_set_named_property(env, options, key, v);
  };

  set_int("windowId", request->window_id);
  set_string("type", request->options.type);
  set_int("x", request->options.x);
  set_int("y", request->options.y);
  set_int("width", request->options.width);
  set_int("height", request->options.height);
  set_bool("show", request->options.show);
  set_bool("resizable", request->options.resizable);
  set_bool("movable", request->options.movable);
  set_bool("minimizable", request->options.minimizable);
  set_bool("maximizable", request->options.maximizable);
  set_bool("closable", request->options.closable);
  set_bool("focusable", request->options.focusable);
  set_bool("alwaysOnTop", request->options.always_on_top);
  set_int("parentWindowId", request->options.parent_window_id);
  set_int("parentHarmonyWindowId", request->options.parent_harmony_window_id);
  set_bool("center", request->options.center);
  set_bool("hasX", request->options.has_x);
  set_bool("hasY", request->options.has_y);
  set_string("title", request->options.title);
  set_bool("fullscreen", request->options.fullscreen);
  set_int("minWidth", request->options.min_width);
  set_int("minHeight", request->options.min_height);
  set_int("maxWidth", request->options.max_width);
  set_int("maxHeight", request->options.max_height);
  set_bool("modal", request->options.modal);
  set_int("displayId", request->options.display_id);

  napi_value argv[1] = {options};
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, js_cb, 1, argv, nullptr);
}

void CreateHarmonyWindowFromNapi(
    int32_t window_id,
    const lynxtron::HarmonyWindowCreationOptions* options) {
  if (!options) return;
  std::lock_guard<std::mutex> lock(g_create_window_mutex);
  if (!g_create_window_tsfn) {
    OH_LOG_ERROR(LOG_APP,
                 "[CreateWindow] no tsfn registered for id=%{public}d",
                 window_id);
    return;
  }
  auto request = std::make_unique<CreateWindowRequest>();
  request->window_id = window_id;
  request->options = *options;
  napi_status status = napi_call_threadsafe_function(
      g_create_window_tsfn, request.release(), napi_tsfn_nonblocking);
  OH_LOG_INFO(LOG_APP,
              "[CreateWindow] posted id=%{public}d type=%{public}s status=%{public}d",
              window_id, options->type.c_str(), (int)status);
}

napi_value RegisterCreateWindowCallback(napi_env env,
                                        napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) {
    napi_throw_error(env, nullptr,
                     "registerCreateWindowCallback requires a function");
    return nullptr;
  }

  napi_valuetype cb_type;
  napi_typeof(env, args[0], &cb_type);
  if (cb_type != napi_function) {
    napi_throw_error(env, nullptr,
                     "registerCreateWindowCallback requires a function");
    return nullptr;
  }

  {
    std::lock_guard<std::mutex> lock(g_create_window_mutex);
    if (g_create_window_tsfn) {
      napi_release_threadsafe_function(g_create_window_tsfn, napi_tsfn_release);
      g_create_window_tsfn = nullptr;
    }

    napi_value async_name;
    napi_create_string_utf8(env, "CreateWindowCallback", NAPI_AUTO_LENGTH,
                            &async_name);

    napi_threadsafe_function tsfn = nullptr;
    napi_status status = napi_create_threadsafe_function(
        env, args[0], nullptr, async_name, 0, 1, nullptr, nullptr, nullptr,
        CreateWindowCallJS, &tsfn);
    if (status != napi_ok) {
      OH_LOG_ERROR(LOG_APP,
                   "napi_create_threadsafe_function failed status=%{public}d",
                   (int)status);
      napi_throw_error(env, nullptr, "failed to create threadsafe function");
      return nullptr;
    }
    g_create_window_tsfn = tsfn;
  }

  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "liblynxtron.so not loaded");
    return nullptr;
  }

  auto set_callback = reinterpret_cast<void (*)(lynxtron::CreateHarmonyWindowCallback)>(
      dlsym(g_lynxtron_handle, "LynxtronSetCreateHarmonyWindowCallback"));
  if (!set_callback) {
    OH_LOG_ERROR(LOG_APP,
                 "dlsym LynxtronSetCreateHarmonyWindowCallback FAILED: %{public}s",
                 dlerror());
    napi_throw_error(env, nullptr,
                     "LynxtronSetCreateHarmonyWindowCallback not found");
    return nullptr;
  }
  set_callback(CreateHarmonyWindowFromNapi);

  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value RegisterWindowOpCallback(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr, nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  int32_t window_id = -1;
  napi_value cb = nullptr;

  if (argc == 2) {
    // registerWindowOpCallback(windowId, callback)
    napi_get_value_int32(env, args[0], &window_id);
    cb = args[1];
  } else if (argc == 1) {
    // Backward-compatible: registerWindowOpCallback(callback) -> windowId 0
    cb = args[0];
    window_id = 0;
  } else {
    napi_throw_error(env, nullptr,
                     "registerWindowOpCallback requires (windowId, callback) "
                     "or (callback)");
    return nullptr;
  }

  napi_valuetype cb_type;
  napi_typeof(env, cb, &cb_type);
  if (cb_type != napi_function) {
    napi_throw_error(env, nullptr,
                     "registerWindowOpCallback requires a function");
    return nullptr;
  }

  // Create or recreate the threadsafe function for this window id.
  {
    std::lock_guard<std::mutex> lock(g_window_op_tsfn_mutex);
    auto it = g_window_op_tsfn_map.find(window_id);
    if (it != g_window_op_tsfn_map.end() && it->second) {
      napi_release_threadsafe_function(it->second, napi_tsfn_release);
    }

    napi_value async_name;
    napi_create_string_utf8(env, "WindowOpCallback", NAPI_AUTO_LENGTH,
                            &async_name);

    napi_threadsafe_function tsfn = nullptr;
    napi_status status = napi_create_threadsafe_function(
        env, cb, nullptr, async_name, 0, 1, nullptr, nullptr, nullptr,
        WindowOpCallJS, &tsfn);
    if (status != napi_ok) {
      OH_LOG_ERROR(LOG_APP,
                   "napi_create_threadsafe_function failed status=%{public}d",
                   (int)status);
      napi_throw_error(env, nullptr, "failed to create threadsafe function");
      return nullptr;
    }
    g_window_op_tsfn_map[window_id] = tsfn;
  }

  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "liblynxtron.so not loaded");
    return nullptr;
  }

  auto fn = reinterpret_cast<LynxtronRegisterWindowOpCallbackFn>(
      dlsym(g_lynxtron_handle, "LynxtronRegisterWindowOpCallbackForWindow"));
  if (!fn) {
    OH_LOG_ERROR(LOG_APP,
                 "dlsym LynxtronRegisterWindowOpCallbackForWindow FAILED: "
                 "%{public}s",
                 dlerror());
    napi_throw_error(env, nullptr,
                     "LynxtronRegisterWindowOpCallbackForWindow not found");
    return nullptr;
  }
  fn(window_id, WindowOpNativeCallback);

  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

// NAPI export getWindowId(): returns liblynxtron's C++ window id, or -1 when
// more than one window exists (multi-window mode).
napi_value GetWindowId(napi_env env, napi_callback_info info) {
  int32_t id = -1;
  if (EnsureLynxtronLoaded()) {
    auto fn = reinterpret_cast<LynxtronGetWindowIdFn>(
        dlsym(g_lynxtron_handle, "LynxtronGetWindowId"));
    if (fn) {
      id = fn();
    } else {
      OH_LOG_ERROR(LOG_APP, "dlsym LynxtronGetWindowId FAILED: %{public}s",
                   dlerror());
    }
  }
  napi_value result = nullptr;
  napi_create_int32(env, id, &result);
  return result;
}

// ---------------------------------------------------------------------------
// OpenExternal handler — bridges platform_util -> ArkTS via TSFN
//
// Pattern: liblynxtron.so exports LynxtronSetOpenExternalHandler (a setter
// that takes a C function pointer).  The bridge dlsym's the setter, injects
// a static C handler, and creates a TSFN so that calls from arbitrary
// lynxtron threads reach the ETS (ArkTS) thread safely.
//
// Electron equivalent: FileAdapter -> AKI GetJSFunction -> ArkTS.
// Lynxtron uses dlsym + TSFN instead of AKI.
// ---------------------------------------------------------------------------

namespace {

// napi_ref to the ArkTS handler registered via registerOpenExternal().
napi_ref g_open_external_js_handler = nullptr;

// TSFN for dispatching from platform_util threads to the ETS main thread.
napi_threadsafe_function g_open_external_tsfn = nullptr;

// Context passed through the TSFN call.
struct OpenExternalData {
  char url[2048];
};

// Called on the ETS main thread by the TSFN.
void OpenExternalCallJs(napi_env env, napi_value js_callback,
                        void* context, void* data) {
  auto* d = static_cast<OpenExternalData*>(data);
  if (!d || !js_callback) {
    OH_LOG_ERROR(LOG_APP, "[OpenExternal] TSFN callback: null data or callback");
    delete d;
    return;
  }

  napi_value args[1];
  napi_status s = napi_create_string_utf8(env, d->url, NAPI_AUTO_LENGTH, &args[0]);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenExternal] napi_create_string_utf8 failed: %{public}d", (int)s);
    delete d;
    return;
  }

  s = napi_call_function(env, nullptr, js_callback, 1, args, nullptr);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenExternal] napi_call_function failed: %{public}d", (int)s);
  } else {
    OH_LOG_INFO(LOG_APP, "[OpenExternal] dispatched url=%{public}s", d->url);
  }
  delete d;
}

// Static C handler injected into liblynxtron.so via LynxtronSetOpenExternalHandler.
// Called from arbitrary lynxtron thread — dispatches via TSFN to ETS thread.
// Returns nullptr (success) immediately; actual result is fire-and-forget
// (matches Electron's out => undefined behavior).
const char* OpenExternalBridgeHandler(const char* url) {
  if (!g_open_external_tsfn || !url) {
    return "openExternal: handler not registered";
  }

  auto* data = new OpenExternalData();
  strncpy(data->url, url, sizeof(data->url) - 1);
  data->url[sizeof(data->url) - 1] = '\0';

  napi_status s = napi_call_threadsafe_function(
      g_open_external_tsfn, data, napi_tsfn_blocking);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenExternal] napi_call_threadsafe_function failed: %{public}d", (int)s);
    delete data;
    return "openExternal: failed to dispatch";
  }

  return nullptr;  // fire-and-forget success
}

// NAPI export: registerOpenExternal(handler)
// ETS calls this at startup to register the URL handler callback.
// The handler receives a URL string and opens it via Want/startAbility.
napi_value RegisterOpenExternal(napi_env env, napi_callback_info info) {
  OH_LOG_INFO(LOG_APP, "[OpenExternal] registerOpenExternal() called from ETS");

  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  if (argc < 1) {
    napi_throw_error(env, nullptr, "registerOpenExternal requires a callback");
    return nullptr;
  }

  napi_valuetype type;
  napi_typeof(env, args[0], &type);
  if (type != napi_function) {
    napi_throw_error(env, nullptr, "registerOpenExternal: arg must be a function");
    return nullptr;
  }

  // Store the JS handler as a napi_ref.
  if (g_open_external_js_handler) {
    napi_delete_reference(env, g_open_external_js_handler);
  }
  napi_create_reference(env, args[0], 1, &g_open_external_js_handler);

  // Create TSFN for cross-thread dispatch.
  if (g_open_external_tsfn) {
    napi_release_threadsafe_function(g_open_external_tsfn, napi_tsfn_release);
  }

  napi_value resource_name;
  napi_create_string_utf8(env, "OpenExternalTSFN", NAPI_AUTO_LENGTH, &resource_name);

  napi_status s = napi_create_threadsafe_function(
      env, args[0], nullptr, resource_name,
      0,   // unlimited queue
      1,   // one thread will call
      nullptr, nullptr, nullptr,
      OpenExternalCallJs,
      &g_open_external_tsfn);

  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenExternal] napi_create_threadsafe_function failed: %{public}d", (int)s);
    napi_throw_error(env, nullptr, "Failed to create TSFN");
    return nullptr;
  }

  OH_LOG_INFO(LOG_APP, "[OpenExternal] TSFN created, handler registered");

  // Inject the static handler into liblynxtron.so.
  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "liblynxtron.so not loaded");
    return nullptr;
  }

  using SetOpenExternalHandlerFn = void (*)(const char* (*)(const char*));
  auto setter = reinterpret_cast<SetOpenExternalHandlerFn>(
      dlsym(g_lynxtron_handle, "LynxtronSetOpenExternalHandler"));
  if (!setter) {
    OH_LOG_ERROR(LOG_APP, "[OpenExternal] dlsym LynxtronSetOpenExternalHandler FAILED: %{public}s",
                 dlerror());
    napi_throw_error(env, nullptr, "LynxtronSetOpenExternalHandler not found");
    return nullptr;
  }

  setter(OpenExternalBridgeHandler);
  OH_LOG_INFO(LOG_APP, "[OpenExternal] handler injected into liblynxtron.so");

  napi_value result;
  napi_get_undefined(env, &result);
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// OpenPath handler — bridges platform_util::OpenPath -> ArkTS via TSFN
//
// Electron reference: FileAdapter::OpenPath() dispatches to two ArkTS methods:
//   directory → FileManagerAdapter.OpenItemInFolder (openLink filemanager)
//   file     → FileManagerAdapter.OpenVerifiedItem (Want viewData + permissions)
//
// Lynxtron: TSFN dispatches (path, isDirectory) to ArkTS.  Fire-and-forget
// like openExternal — the ArkTS calls (openLink / startAbility) return
// immediately, so no timeout/promise machinery needed for v1.
// ---------------------------------------------------------------------------

namespace {

napi_ref g_open_path_js_handler = nullptr;
napi_threadsafe_function g_open_path_tsfn = nullptr;

struct OpenPathData {
  char path[4096];
  int is_directory;
};

void OpenPathCallJs(napi_env env, napi_value js_callback,
                    void* context, void* data) {
  auto* d = static_cast<OpenPathData*>(data);
  if (!d || !js_callback) {
    OH_LOG_ERROR(LOG_APP, "[OpenPath] TSFN callback: null");
    delete d;
    return;
  }

  napi_value args[2];
  napi_create_string_utf8(env, d->path, NAPI_AUTO_LENGTH, &args[0]);
  napi_create_int32(env, d->is_directory, &args[1]);

  napi_status s = napi_call_function(env, nullptr, js_callback, 2, args, nullptr);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenPath] napi_call_function failed: %{public}d", (int)s);
  } else {
    OH_LOG_INFO(LOG_APP, "[OpenPath] dispatched path=%{public}s isDir=%{public}d",
                d->path, d->is_directory);
  }
  delete d;
}

const char* OpenPathBridgeHandler(const char* path, int is_directory) {
  if (!g_open_path_tsfn || !path) {
    return "openPath: handler not registered";
  }

  auto* data = new OpenPathData();
  strncpy(data->path, path, sizeof(data->path) - 1);
  data->path[sizeof(data->path) - 1] = '\0';
  data->is_directory = is_directory;

  napi_status s = napi_call_threadsafe_function(
      g_open_path_tsfn, data, napi_tsfn_blocking);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenPath] TSFN dispatch failed: %{public}d", (int)s);
    delete data;
    return "openPath: failed to dispatch";
  }
  return nullptr;
}

napi_value RegisterOpenPath(napi_env env, napi_callback_info info) {
  OH_LOG_INFO(LOG_APP, "[OpenPath] registerOpenPath() called from ETS");

  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  if (argc < 1) {
    napi_throw_error(env, nullptr, "registerOpenPath requires a callback");
    return nullptr;
  }

  napi_valuetype type;
  napi_typeof(env, args[0], &type);
  if (type != napi_function) {
    napi_throw_error(env, nullptr, "registerOpenPath: arg must be a function");
    return nullptr;
  }

  if (g_open_path_js_handler) {
    napi_delete_reference(env, g_open_path_js_handler);
  }
  napi_create_reference(env, args[0], 1, &g_open_path_js_handler);

  if (g_open_path_tsfn) {
    napi_release_threadsafe_function(g_open_path_tsfn, napi_tsfn_release);
  }

  napi_value resource_name;
  napi_create_string_utf8(env, "OpenPathTSFN", NAPI_AUTO_LENGTH, &resource_name);

  napi_status s = napi_create_threadsafe_function(
      env, args[0], nullptr, resource_name,
      0, 1, nullptr, nullptr, nullptr,
      OpenPathCallJs, &g_open_path_tsfn);

  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenPath] TSFN create failed: %{public}d", (int)s);
    napi_throw_error(env, nullptr, "Failed to create OpenPath TSFN");
    return nullptr;
  }

  OH_LOG_INFO(LOG_APP, "[OpenPath] TSFN created, injecting handler...");

  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "liblynxtron.so not loaded");
    return nullptr;
  }

  using SetOpenPathHandlerFn = void (*)(const char* (*)(const char*, int));
  auto setter = reinterpret_cast<SetOpenPathHandlerFn>(
      dlsym(g_lynxtron_handle, "LynxtronSetOpenPathHandler"));
  if (!setter) {
    OH_LOG_ERROR(LOG_APP, "[OpenPath] dlsym LynxtronSetOpenPathHandler FAILED: %{public}s",
                 dlerror());
    napi_throw_error(env, nullptr, "LynxtronSetOpenPathHandler not found");
    return nullptr;
  }

  setter(OpenPathBridgeHandler);
  OH_LOG_INFO(LOG_APP, "[OpenPath] handler injected into liblynxtron.so");

  napi_value result;
  napi_get_undefined(env, &result);
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// ShowOpenDialog handler — bridges file_dialog::ShowOpenDialog -> ArkTS
//
// Electron reference: DialogAdapter::ShowOpenDialog() runs a std::thread +
// std::promise/future, invokes the ArkTS DocumentViewPicker via AKI, and
// blocks until the ArkTS callback resolves the promise.
//
// Lynxtron: file_dialog_harmony.cc holds a promise/future and calls the
// injected handler below.  The handler allocates a request id, dispatches
// (id, settings_json) to ArkTS through a TSFN, and returns immediately;
// liblynxtron.so keeps blocking on its future.  When ArkTS finishes the
// picker it calls resolveShowOpenDialog(id, uris, paths, canceled), which looks up
// the request and fires the stored callback — unblocking the C++ side.
// ---------------------------------------------------------------------------
namespace {

using ShowOpenDialogResultCallback = void (*)(void* user_data,
                                              const char* const* uris,
                                              size_t uri_count,
                                              const char* const* paths,
                                              size_t path_count,
                                              bool canceled);
using ShowOpenDialogHandlerFn = void (*)(const char* settings_json,
                                         ShowOpenDialogResultCallback callback,
                                         void* user_data);

napi_ref g_show_open_dialog_js_handler = nullptr;
napi_threadsafe_function g_show_open_dialog_tsfn = nullptr;

struct ShowOpenDialogRequest {
  ShowOpenDialogResultCallback callback;
  void* user_data;
};

struct ShowOpenDialogTsfnData {
  int id;
  std::string settings;
};

std::mutex g_dialog_mutex;
std::unordered_map<int, ShowOpenDialogRequest> g_dialog_requests;
int g_next_dialog_id = 1;

// Called on the ETS main thread by the TSFN: delivers (id, settings) to the
// ArkTS FileDialogBridge handler.
void ShowOpenDialogCallJs(napi_env env, napi_value js_callback,
                          void* context, void* data) {
  auto* d = static_cast<ShowOpenDialogTsfnData*>(data);
  if (!d || !js_callback) {
    OH_LOG_ERROR(LOG_APP, "[OpenDialog] TSFN callback: null data or callback");
    delete d;
    return;
  }

  napi_value args[2];
  napi_create_int32(env, d->id, &args[0]);
  napi_create_string_utf8(env, d->settings.c_str(), NAPI_AUTO_LENGTH,
                          &args[1]);

  napi_status s = napi_call_function(env, nullptr, js_callback, 2, args,
                                     nullptr);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenDialog] napi_call_function failed: %{public}d",
                 (int)s);
  } else {
  }
  delete d;
}

// Static C handler injected into liblynxtron.so via
// LynxtronSetShowOpenDialogHandler.  Called from the thread-pool thread where
// file_dialog::ShowOpenDialogAdapter blocks on its future.  We only dispatch
// to ArkTS and return; the future is resolved later by ResolveShowOpenDialog.
void ShowOpenDialogBridgeHandler(const char* settings_json,
                                 ShowOpenDialogResultCallback callback,
                                 void* user_data) {
  if (!g_show_open_dialog_tsfn || !callback) {
    OH_LOG_ERROR(LOG_APP, "[OpenDialog] handler: tsfn or callback missing");
    if (callback) callback(user_data, nullptr, 0, nullptr, 0, true);
    return;
  }

  int id;
  {
    std::lock_guard<std::mutex> lock(g_dialog_mutex);
    id = g_next_dialog_id++;
    g_dialog_requests[id] = {callback, user_data};
  }

  auto* data = new ShowOpenDialogTsfnData();
  data->id = id;
  if (settings_json) data->settings = settings_json;

  napi_status s = napi_call_threadsafe_function(g_show_open_dialog_tsfn, data,
                                                napi_tsfn_blocking);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenDialog] TSFN dispatch failed: %{public}d",
                 (int)s);
    {
      std::lock_guard<std::mutex> lock(g_dialog_mutex);
      g_dialog_requests.erase(id);
    }
    delete data;
    callback(user_data, nullptr, 0, nullptr, 0, true);  // treat as canceled
  }
}

// NAPI export: resolveShowOpenDialog(id, uris, paths, canceled)
// Called from ArkTS on the ETS main thread after the picker finishes.
// uris = raw picker URIs (for FileAccessPersist), paths = converted real
// paths (returned to JS as filePaths).
napi_value ResolveShowOpenDialog(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 4) {
    napi_throw_error(env, nullptr,
                     "resolveShowOpenDialog requires (id, uris, paths, canceled)");
    return nullptr;
  }

  int32_t id = -1;
  napi_get_value_int32(env, args[0], &id);
  bool canceled = true;
  napi_get_value_bool(env, args[3], &canceled);

  ShowOpenDialogRequest req;
  {
    std::lock_guard<std::mutex> lock(g_dialog_mutex);
    auto it = g_dialog_requests.find(id);
    if (it == g_dialog_requests.end()) {
      OH_LOG_ERROR(LOG_APP, "[OpenDialog] resolve: unknown id=%{public}d", id);
      napi_value result;
      napi_get_undefined(env, &result);
      return result;
    }
    req = it->second;
    g_dialog_requests.erase(it);
  }

  std::vector<std::string> uris;
  std::vector<std::string> paths;
  if (!canceled) {
    auto parse_array = [&](napi_value arr, std::vector<std::string>& out) {
      bool is_array = false;
      napi_is_array(env, arr, &is_array);
      if (!is_array) {
        return;
      }
      uint32_t len = 0;
      napi_get_array_length(env, arr, &len);
      for (uint32_t i = 0; i < len; ++i) {
        napi_value elem;
        if (napi_get_element(env, arr, i, &elem) != napi_ok) continue;
        size_t slen = 0;
        napi_get_value_string_utf8(env, elem, nullptr, 0, &slen);
        if (slen == 0) continue;
        std::string s(slen + 1, '\0');
        napi_get_value_string_utf8(env, elem, s.data(), s.size(), &slen);
        s.resize(slen);
        out.push_back(std::move(s));
      }
    };
    parse_array(args[1], uris);
    parse_array(args[2], paths);
  }

  std::vector<const char*> c_uris;
  c_uris.reserve(uris.size());
  for (const auto& u : uris) c_uris.push_back(u.c_str());
  std::vector<const char*> c_paths;
  c_paths.reserve(paths.size());
  for (const auto& p : paths) c_paths.push_back(p.c_str());

  if (req.callback) {
    req.callback(req.user_data, c_uris.data(), c_uris.size(), c_paths.data(),
                 c_paths.size(), canceled);
  }

  napi_value result;
  napi_get_undefined(env, &result);
  return result;
}

// NAPI export: registerShowOpenDialog(handler)
// ETS calls this at startup to register the (id, settings) handler and
// inject ShowOpenDialogBridgeHandler into liblynxtron.so.
napi_value RegisterShowOpenDialog(napi_env env, napi_callback_info info) {

  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) {
    napi_throw_error(env, nullptr, "registerShowOpenDialog requires a callback");
    return nullptr;
  }

  napi_valuetype type;
  napi_typeof(env, args[0], &type);
  if (type != napi_function) {
    napi_throw_error(env, nullptr,
                     "registerShowOpenDialog: arg must be a function");
    return nullptr;
  }

  if (g_show_open_dialog_js_handler) {
    napi_delete_reference(env, g_show_open_dialog_js_handler);
  }
  napi_create_reference(env, args[0], 1, &g_show_open_dialog_js_handler);

  if (g_show_open_dialog_tsfn) {
    napi_release_threadsafe_function(g_show_open_dialog_tsfn,
                                     napi_tsfn_release);
  }

  napi_value resource_name;
  napi_create_string_utf8(env, "ShowOpenDialogTSFN", NAPI_AUTO_LENGTH,
                          &resource_name);
  napi_status s = napi_create_threadsafe_function(
      env, args[0], nullptr, resource_name,
      0, 1, nullptr, nullptr, nullptr,
      ShowOpenDialogCallJs, &g_show_open_dialog_tsfn);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[OpenDialog] TSFN create failed: %{public}d",
                 (int)s);
    napi_throw_error(env, nullptr, "Failed to create ShowOpenDialog TSFN");
    return nullptr;
  }

  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "liblynxtron.so not loaded");
    return nullptr;
  }

  using SetShowOpenDialogHandlerFn = void (*)(ShowOpenDialogHandlerFn);
  auto setter = reinterpret_cast<SetShowOpenDialogHandlerFn>(
      dlsym(g_lynxtron_handle, "LynxtronSetShowOpenDialogHandler"));
  if (!setter) {
    OH_LOG_ERROR(LOG_APP,
                 "[OpenDialog] dlsym LynxtronSetShowOpenDialogHandler FAILED: "
                 "%{public}s",
                 dlerror());
    napi_throw_error(env, nullptr, "LynxtronSetShowOpenDialogHandler not found");
    return nullptr;
  }

  setter(ShowOpenDialogBridgeHandler);

  napi_value result;
  napi_get_undefined(env, &result);
  return result;
}

// ---------------------------------------------------------------------------
// Save dialog bridge (showSaveDialog)
//
// Mirrors the open-dialog bridge above.  Save returns a single path, so the
// result callback carries one uri + one converted path instead of arrays.
// ---------------------------------------------------------------------------

using ShowSaveDialogResultCallback = void (*)(void* user_data,
                                              const char* uri,
                                              const char* path,
                                              bool canceled);
using ShowSaveDialogHandlerFn = void (*)(const char* settings_json,
                                         ShowSaveDialogResultCallback callback,
                                         void* user_data);

napi_ref g_show_save_dialog_js_handler = nullptr;
napi_threadsafe_function g_show_save_dialog_tsfn = nullptr;

struct ShowSaveDialogRequest {
  ShowSaveDialogResultCallback callback;
  void* user_data;
};

struct ShowSaveDialogTsfnData {
  int id;
  std::string settings;
};

std::mutex g_save_dialog_mutex;
std::unordered_map<int, ShowSaveDialogRequest> g_save_dialog_requests;
int g_next_save_dialog_id = 1;

// Called on the ETS main thread by the TSFN: delivers (id, settings) to the
// ArkTS FileDialogBridge.save handler.
void ShowSaveDialogCallJs(napi_env env, napi_value js_callback,
                          void* context, void* data) {
  auto* d = static_cast<ShowSaveDialogTsfnData*>(data);
  if (!d || !js_callback) {
    OH_LOG_ERROR(LOG_APP, "[SaveDialog] TSFN callback: null data or callback");
    delete d;
    return;
  }

  napi_value args[2];
  napi_create_int32(env, d->id, &args[0]);
  napi_create_string_utf8(env, d->settings.c_str(), NAPI_AUTO_LENGTH,
                          &args[1]);

  napi_status s = napi_call_function(env, nullptr, js_callback, 2, args,
                                     nullptr);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[SaveDialog] napi_call_function failed: %{public}d",
                 (int)s);
  }
  delete d;
}

// Static C handler injected into liblynxtron.so via
// LynxtronSetShowSaveDialogHandler.  Same dispatch pattern as the open dialog.
void ShowSaveDialogBridgeHandler(const char* settings_json,
                                 ShowSaveDialogResultCallback callback,
                                 void* user_data) {
  if (!g_show_save_dialog_tsfn || !callback) {
    OH_LOG_ERROR(LOG_APP, "[SaveDialog] handler: tsfn or callback missing");
    if (callback) callback(user_data, nullptr, nullptr, true);
    return;
  }

  int id;
  {
    std::lock_guard<std::mutex> lock(g_save_dialog_mutex);
    id = g_next_save_dialog_id++;
    g_save_dialog_requests[id] = {callback, user_data};
  }

  auto* data = new ShowSaveDialogTsfnData();
  data->id = id;
  if (settings_json) data->settings = settings_json;

  napi_status s = napi_call_threadsafe_function(g_show_save_dialog_tsfn, data,
                                                napi_tsfn_blocking);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[SaveDialog] TSFN dispatch failed: %{public}d",
                 (int)s);
    {
      std::lock_guard<std::mutex> lock(g_save_dialog_mutex);
      g_save_dialog_requests.erase(id);
    }
    delete data;
    callback(user_data, nullptr, nullptr, true);  // treat as canceled
  }
}

// NAPI export: resolveShowSaveDialog(id, uri, path, canceled)
// Called from ArkTS on the ETS main thread after the save picker finishes.
napi_value ResolveShowSaveDialog(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 4) {
    napi_throw_error(env, nullptr,
                     "resolveShowSaveDialog requires (id, uri, path, canceled)");
    return nullptr;
  }

  int32_t id = -1;
  napi_get_value_int32(env, args[0], &id);
  bool canceled = true;
  napi_get_value_bool(env, args[3], &canceled);

  ShowSaveDialogRequest req;
  {
    std::lock_guard<std::mutex> lock(g_save_dialog_mutex);
    auto it = g_save_dialog_requests.find(id);
    if (it == g_save_dialog_requests.end()) {
      OH_LOG_ERROR(LOG_APP, "[SaveDialog] resolve: unknown id=%{public}d", id);
      napi_value result;
      napi_get_undefined(env, &result);
      return result;
    }
    req = it->second;
    g_save_dialog_requests.erase(it);
  }

  std::string uri;
  std::string path;
  if (!canceled) {
    auto get_str = [&](napi_value v, std::string& out) -> bool {
      size_t slen = 0;
      if (napi_get_value_string_utf8(env, v, nullptr, 0, &slen) != napi_ok ||
          slen == 0) {
        return false;
      }
      std::string s(slen + 1, '\0');
      napi_get_value_string_utf8(env, v, s.data(), s.size(), &slen);
      s.resize(slen);
      out = std::move(s);
      return true;
    };
    get_str(args[1], uri);
    get_str(args[2], path);
  }

  if (req.callback) {
    req.callback(req.user_data, uri.empty() ? nullptr : uri.c_str(),
                 path.empty() ? nullptr : path.c_str(), canceled);
  }

  napi_value result;
  napi_get_undefined(env, &result);
  return result;
}

// NAPI export: registerShowSaveDialog(handler)
// ETS calls this at startup to register the save handler and inject
// ShowSaveDialogBridgeHandler into liblynxtron.so.
napi_value RegisterShowSaveDialog(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (argc < 1) {
    napi_throw_error(env, nullptr, "registerShowSaveDialog requires a callback");
    return nullptr;
  }

  napi_valuetype type;
  napi_typeof(env, args[0], &type);
  if (type != napi_function) {
    napi_throw_error(env, nullptr,
                     "registerShowSaveDialog: arg must be a function");
    return nullptr;
  }

  if (g_show_save_dialog_js_handler) {
    napi_delete_reference(env, g_show_save_dialog_js_handler);
  }
  napi_create_reference(env, args[0], 1, &g_show_save_dialog_js_handler);

  if (g_show_save_dialog_tsfn) {
    napi_release_threadsafe_function(g_show_save_dialog_tsfn,
                                     napi_tsfn_release);
  }

  napi_value resource_name;
  napi_create_string_utf8(env, "ShowSaveDialogTSFN", NAPI_AUTO_LENGTH,
                          &resource_name);
  napi_status s = napi_create_threadsafe_function(
      env, args[0], nullptr, resource_name,
      0, 1, nullptr, nullptr, nullptr,
      ShowSaveDialogCallJs, &g_show_save_dialog_tsfn);
  if (s != napi_ok) {
    OH_LOG_ERROR(LOG_APP, "[SaveDialog] TSFN create failed: %{public}d",
                 (int)s);
    napi_throw_error(env, nullptr, "Failed to create ShowSaveDialog TSFN");
    return nullptr;
  }

  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "liblynxtron.so not loaded");
    return nullptr;
  }

  using SetShowSaveDialogHandlerFn = void (*)(ShowSaveDialogHandlerFn);
  auto setter = reinterpret_cast<SetShowSaveDialogHandlerFn>(
      dlsym(g_lynxtron_handle, "LynxtronSetShowSaveDialogHandler"));
  if (!setter) {
    OH_LOG_ERROR(LOG_APP,
                 "[SaveDialog] dlsym LynxtronSetShowSaveDialogHandler FAILED: "
                 "%{public}s",
                 dlerror());
    napi_throw_error(env, nullptr, "LynxtronSetShowSaveDialogHandler not found");
    return nullptr;
  }

  setter(ShowSaveDialogBridgeHandler);

  napi_value result;
  napi_get_undefined(env, &result);
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// XComponent surface plumbing
//
// The ETS <XComponent type="surface" libraryname="lynxtron_napi"/> hands its
// native EGLNativeWindow (an OHNativeWindow*) to OnSurfaceCreated below. For
// bring-up we bring up EGL on that window and clear it to a solid color to
// prove the surface reaches the screen. Once the Lynx/Chromium GL compositor
// is wired, this surface becomes NativeWindowHarmony::surface_ and the
// compositor's EGLSurface renders here instead of the test clear.
// ---------------------------------------------------------------------------

void* g_native_window = nullptr;

// The HarmonyOS window id of the Ability whose XComponent lifecycle is
// currently running on this thread. Written by SetWindowId / SetWindowIdForWindow
// and read by OnSurfaceCreated/OnSurfaceChanged to route surface updates to the
// correct NativeWindowHarmony. One UIAbility runs on its own JS/UI thread, so
// thread_local is sufficient.
thread_local int32_t g_current_harmony_window_id = -1;

// Resolved from liblynxtron.so (loaded via dlopen in EnsureLynxtronLoaded).
// Skia GL rendering lives in the main library where Skia is linked.
using SetSurfaceFn = void (*)(int32_t harmony_window_id,
                              void* window,
                              int width,
                              int height);
SetSurfaceFn g_set_surface = nullptr;

// LynxtronSendPointerEventForWindow(harmony_window_id, phase, x, y, buttons,
// device, kind, timestamp) — forwards input to the LynxView's windowless
// renderer for the given window.
// phase: 0=down,1=up,2=move,3=hover.
using SendPointerFn = void (*)(int32_t harmony_window_id,
                               int phase, double x, double y, int64_t buttons,
                               int32_t device, int kind, size_t timestamp);
SendPointerFn g_send_pointer = nullptr;

using SendScrollFn = void (*)(int32_t harmony_window_id, double x, double y,
                              double delta_x, double delta_y,
                              size_t timestamp, bool precise);
SendScrollFn g_send_scroll = nullptr;

// ArkUI reports mouse events with no device id and may report the primary
// touch contact as id 0.  Lynx keys pointer state by `device`, so sharing 0
// makes a touch Down reuse the mouse Hover pointer and prevents a tap from
// getting its required Add -> Down -> Up sequence.
constexpr int32_t kLynxtronMouseDeviceId = 1;
constexpr int32_t kLynxtronTouchDeviceIdBase = 1000;

// OH_NativeXComponent_MouseEvent::button identifies the button that changed
// for press/release events.  It is normally NONE for a MOVE event, even while
// the user is holding the primary button down.  Keep the state per Harmony
// window so a desktop drag is delivered to Lynx as Down -> Move -> Up rather
// than Down -> Hover -> Up.  The latter prevents ScrollView/List from
// recognizing a pan gesture.
std::mutex g_mouse_buttons_mutex;
std::unordered_map<int32_t, int64_t> g_mouse_buttons_by_window;

using SendKeyFn = void (*)(int32_t harmony_window_id,
                           int type, uint64_t logical, uint64_t physical,
                           double timestamp);
using SendTextFn = void (*)(int32_t harmony_window_id,
                            const char* text, double timestamp);
using GetTextInputStateFn = bool (*)(int32_t harmony_window_id,
                                     float*, float*, float*, float*);
using GetTitleFn = const char* (*)();
using SetWindowCommandHandlerFn = void (*)(void (*)(int));
SendKeyFn g_send_key = nullptr;
SendTextFn g_send_text = nullptr;
SendTextFn g_send_composing_text = nullptr;
GetTextInputStateFn g_get_text_input_state = nullptr;
GetTitleFn g_get_title = nullptr;
SetWindowCommandHandlerFn g_set_window_command_handler = nullptr;

// ---- Update check / AppGallery Kit bridge ----
// Uses NAPI ThreadSafe Function for instant Node.js → ArkTS communication.

#define UPDATEMODEL_LOG(fmt, ...) OH_LOG_INFO(LOG_APP, "[UpdateModel lynxtron_napi_bridge.cc] " fmt, ##__VA_ARGS__)

using ResolveCheckFn = void (*)(const char*);
using ResolveDialogFn = void (*)(int);
using ResolveProductFn = void (*)(const char*);
using RegisterTSFNFn = void (*)(void* env, void* tsfn);

ResolveCheckFn g_resolve_check = nullptr;
ResolveDialogFn g_resolve_dialog = nullptr;
ResolveProductFn g_resolve_product = nullptr;
napi_threadsafe_function g_update_tsfn = nullptr;

// Called by ArkTS at init time: lynxtron.registerUpdateTSFN(callback).
// Creates a NAPI ThreadSafe Function that Node.js can wake from any thread,
// then registers it with the C++ binding via dlsym.
napi_value RegisterUpdateTSFN(napi_env env, napi_callback_info info) {
  UPDATEMODEL_LOG("RegisterUpdateTSFN ENTER");
  // Ensure liblynxtron.so is loaded so we can dlsym LynxtronRegisterUpdateTSFN.
  // aboutToAppear() runs before XComponent.onLoad → Start() → EnsureLynxtronLoaded(),
  // so g_lynxtron_handle may still be null here.
  EnsureLynxtronLoaded();
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 1) {
    OH_LOG_ERROR(LOG_APP, "[UpdateModel lynxtron_napi_bridge.cc] RegisterUpdateTSFN: missing callback arg");
    napi_value r; napi_get_undefined(env, &r); return r;
  }
  // Create TSFN from the ArkTS callback.
  napi_value res_name;
  napi_create_string_utf8(env, "UpdateTSFN", NAPI_AUTO_LENGTH, &res_name);
  napi_create_threadsafe_function(
      env, argv[0], nullptr, res_name, 4, 1, nullptr,
      [](napi_env, void*, void* ctx) {
        // TSFN finalizer — cleanup context if needed.
      },
      nullptr,
      [](napi_env tsfn_env, napi_value js_callback, void* context, void* data) {
        // Called on ArkTS main thread when Node.js pushes a request.
        auto* type_ptr = static_cast<int*>(data);
        int request_type = *type_ptr;
        delete type_ptr;
        UPDATEMODEL_LOG("TSFN callback: type=%{public}d", request_type);
        napi_value argv_cb[1];
        napi_create_int32(tsfn_env, request_type, &argv_cb[0]);
        napi_value global;
        napi_get_global(tsfn_env, &global);
        napi_call_function(tsfn_env, global, js_callback, 1, argv_cb, nullptr);
      },
      &g_update_tsfn);
  UPDATEMODEL_LOG("RegisterUpdateTSFN: TSFN created tsfn=%{public}p",
              (void*)g_update_tsfn);

  // Register the TSFN with the C++ binding so Node.js can use it.
  if (g_lynxtron_handle) {
    auto reg = reinterpret_cast<RegisterTSFNFn>(
        dlsym(g_lynxtron_handle, "LynxtronRegisterUpdateTSFN"));
    if (reg) {
      reg(env, g_update_tsfn);
      UPDATEMODEL_LOG("RegisterUpdateTSFN: registered with C++ binding");
    } else {
      OH_LOG_ERROR(LOG_APP, "[UpdateModel lynxtron_napi_bridge.cc] RegisterUpdateTSFN: dlsym LynxtronRegisterUpdateTSFN failed: %{public}s", dlerror());
    }
  } else {
    OH_LOG_ERROR(LOG_APP, "[UpdateModel lynxtron_napi_bridge.cc] RegisterUpdateTSFN: g_lynxtron_handle is null even after EnsureLynxtronLoaded");
  }

  napi_value r; napi_get_undefined(env, &r); return r;
}

// Lynx logical key ids (see ToLynxLogicalKey below for the full mapping).
constexpr uint64_t kLogicalBackspace = 0x00100000008ULL;
constexpr uint64_t kLogicalEnter = 0x0010000000dULL;
constexpr uint64_t kLogicalDelete = 0x0010000007fULL;
constexpr uint64_t kLogicalArrowDown = 0x00100000301ULL;
constexpr uint64_t kLogicalArrowLeft = 0x00100000302ULL;
constexpr uint64_t kLogicalArrowRight = 0x00100000303ULL;
constexpr uint64_t kLogicalArrowUp = 0x00100000304ULL;

double NowMicros() {
  return std::chrono::duration<double, std::micro>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void ForwardPointer(int32_t harmony_window_id,
                    int phase, double x, double y, int64_t buttons,
                    int32_t device, int kind, size_t timestamp) {
  OH_LOG_INFO(LOG_APP, "[XC] pointer phase=%{public}d x=%{public}f y=%{public}f b=%{public}lld id=%{public}d",
              phase, x, y, (long long)buttons, harmony_window_id);
  if (!g_send_pointer) {
    if (!g_lynxtron_handle) {
      OH_LOG_ERROR(LOG_APP, "[XC] ForwardPointer: liblynxtron.so not loaded");
      return;
    }
    g_send_pointer = reinterpret_cast<SendPointerFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendPointerEventForWindow"));
    if (!g_send_pointer) {
      OH_LOG_ERROR(LOG_APP,
                   "[XC] ForwardPointer: dlsym LynxtronSendPointerEventForWindow FAILED: "
                   "%{public}s",
                   dlerror());
      return;
    }
  }
  g_send_pointer(harmony_window_id, phase, x, y, buttons, device, kind, timestamp);
}

void ForwardScroll(int32_t harmony_window_id, double x, double y,
                   double delta_x, double delta_y, bool precise,
                   size_t timestamp) {
  if (!g_lynxtron_handle) return;
  if (!g_send_scroll) {
    g_send_scroll = reinterpret_cast<SendScrollFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendScrollEventForWindow"));
  }
  if (!g_send_scroll) {
    OH_LOG_ERROR(LOG_APP,
                 "[XC] dlsym LynxtronSendScrollEventForWindow failed: %{public}s",
                 dlerror());
    return;
  }
  OH_LOG_INFO(LOG_APP,
              "[XC] scroll x=%{public}f y=%{public}f dx=%{public}f dy=%{public}f",
              x, y, delta_x, delta_y);
  g_send_scroll(harmony_window_id, x, y, delta_x, delta_y, timestamp, precise);
}

// Try to read the HarmonyOS window id encoded in the XComponent id string
// (e.g. "lynxtron_surface_12345"). Falls back to -1 if the id is missing or
// not in the expected format.
int32_t GetSurfaceWindowId(OH_NativeXComponent* component) {
  if (!component) {
    return -1;
  }
  char id_buf[128] = {0};
  uint64_t len = sizeof(id_buf);
  if (OH_NativeXComponent_GetXComponentId(component, id_buf, &len) != 0) {
    return -1;
  }
  std::string id(id_buf);
  const std::string prefix = "lynxtron_surface_";
  if (id.compare(0, prefix.size(), prefix) != 0) {
    return -1;
  }
  std::string suffix = id.substr(prefix.size());
  if (suffix.empty()) {
    return -1;
  }
  errno = 0;
  char* endptr = nullptr;
  long long value = std::strtoll(suffix.c_str(), &endptr, 10);
  if (endptr == suffix.c_str() || *endptr != '\0' || errno != 0 ||
      value <= 0 || value > std::numeric_limits<int32_t>::max()) {
    return -1;
  }
  return static_cast<int32_t>(value);
}

void ForwardSurface(OH_NativeXComponent* component, void* window) {
  if (!window) return;

  uint64_t w = 0, h = 0;
  OH_NativeXComponent_GetXComponentSize(component, window, &w, &h);
  OH_LOG_INFO(LOG_APP, "[XC] surface size %{public}llux%{public}llu",
              (unsigned long long)w, (unsigned long long)h);

  if (!g_set_surface) {
    if (!EnsureLynxtronLoaded()) {
      OH_LOG_ERROR(LOG_APP, "[XC] liblynxtron.so not loaded, cannot render");
      return;
    }
    g_set_surface = reinterpret_cast<SetSurfaceFn>(
        dlsym(g_lynxtron_handle, "LynxtronSetNativeSurface"));
    if (!g_set_surface) {
      OH_LOG_ERROR(LOG_APP, "[XC] dlsym LynxtronSetNativeSurface FAILED: "
                   "%{public}s", dlerror());
      return;
    }
  }
  int32_t surface_window_id = GetSurfaceWindowId(component);
  if (surface_window_id <= 0) {
    surface_window_id = g_current_harmony_window_id;
    if (surface_window_id <= 0) {
      OH_LOG_WARN(LOG_APP,
                  "[XC] ForwardSurface: no XComponent id or TLS window id, "
                  "falling back to single-window routing");
    }
  }
  g_set_surface(surface_window_id, window, static_cast<int>(w), static_cast<int>(h));
}

void OnSurfaceCreated(OH_NativeXComponent* component, void* window) {
  OH_LOG_INFO(LOG_APP, "[XC] OnSurfaceCreated window=%{public}p", window);
  g_native_window = window;
  ForwardSurface(component, window);
}

void OnSurfaceChanged(OH_NativeXComponent* component, void* window) {
  OH_LOG_INFO(LOG_APP, "[XC] OnSurfaceChanged");
  ForwardSurface(component, window);
}

void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window) {
  OH_LOG_INFO(LOG_APP, "[XC] OnSurfaceDestroyed");
  const int32_t surface_window_id = GetSurfaceWindowId(component);
  std::lock_guard<std::mutex> lock(g_mouse_buttons_mutex);
  g_mouse_buttons_by_window.erase(surface_window_id);
  g_native_window = nullptr;
}

void DispatchTouchEvent(OH_NativeXComponent* component, void* window) {
  OH_NativeXComponent_TouchEvent te;
  if (OH_NativeXComponent_GetTouchEvent(component, window, &te) != 0) return;
  int phase;
  int64_t buttons = 1;  // touch acts as primary button
  switch (te.type) {
    case OH_NATIVEXCOMPONENT_DOWN:
      phase = 0;
      break;
    case OH_NATIVEXCOMPONENT_UP:
      phase = 1;
      buttons = 0;
      break;
    case OH_NATIVEXCOMPONENT_MOVE:
      phase = 2;
      break;
    case OH_NATIVEXCOMPONENT_CANCEL:
      phase = 4;
      buttons = 0;
      break;
    default:
      return;
  }
  // Use the same microsecond clock as the other Lynx embedders. ArkUI's event
  // timestamp uses a platform-specific unit and is not comparable to Clay's.
  const size_t timestamp = static_cast<size_t>(NowMicros());
  const int32_t device = kLynxtronTouchDeviceIdBase +
                         static_cast<int32_t>(te.deviceId);
  int32_t surface_window_id = GetSurfaceWindowId(component);
  if (surface_window_id <= 0) {
    surface_window_id = g_current_harmony_window_id;
  }
  ForwardPointer(surface_window_id, phase, te.x, te.y, buttons, device,
                 /*touch=*/2, timestamp);
  // Do not emit Remove immediately after Up.  Clay maps Remove to Cancel
  // before the Lynx event dispatcher sees it, which cancels bindtap after a
  // valid Up.  Keep the device registered, as the desktop embedders do, and
  // reuse it for its next touch sequence.
}

OH_NativeXComponent_Callback g_xc_callback = {
    .OnSurfaceCreated = OnSurfaceCreated,
    .OnSurfaceChanged = OnSurfaceChanged,
    .OnSurfaceDestroyed = OnSurfaceDestroyed,
    .DispatchTouchEvent = DispatchTouchEvent,
};

// ---- Mouse (HarmonyOS PC) ----
void DispatchMouseEvent(OH_NativeXComponent* component, void* window) {
  OH_NativeXComponent_MouseEvent me;
  if (OH_NativeXComponent_GetMouseEvent(component, window, &me) != 0) return;
  int64_t changed_button = 0;
  if (me.button == OH_NATIVEXCOMPONENT_LEFT_BUTTON) changed_button = 1;
  else if (me.button == OH_NATIVEXCOMPONENT_RIGHT_BUTTON) changed_button = 2;
  else if (me.button == OH_NATIVEXCOMPONENT_MIDDLE_BUTTON) changed_button = 4;

  int32_t surface_window_id = GetSurfaceWindowId(component);
  if (surface_window_id <= 0) {
    surface_window_id = g_current_harmony_window_id;
  }

  int64_t buttons = 0;
  int phase;
  {
    std::lock_guard<std::mutex> lock(g_mouse_buttons_mutex);
    int64_t& tracked_buttons = g_mouse_buttons_by_window[surface_window_id];
    switch (me.action) {
      case OH_NATIVEXCOMPONENT_MOUSE_PRESS:
        // A missing button is treated as primary for compatibility with older
        // HarmonyOS PC builds that omit it for the first press packet.
        tracked_buttons |= changed_button != 0 ? changed_button : 1;
        phase = 0;
        break;
      case OH_NATIVEXCOMPONENT_MOUSE_RELEASE:
        tracked_buttons &= ~(changed_button != 0 ? changed_button : 1);
        phase = 1;
        break;
      case OH_NATIVEXCOMPONENT_MOUSE_MOVE:
        phase = tracked_buttons != 0 ? 2 : 3;  // drag vs hover
        break;
      case OH_NATIVEXCOMPONENT_MOUSE_CANCEL:
        tracked_buttons = 0;
        phase = 4;
        break;
      default:
        return;
    }
    buttons = tracked_buttons;
    if (tracked_buttons == 0 &&
        (me.action == OH_NATIVEXCOMPONENT_MOUSE_RELEASE ||
         me.action == OH_NATIVEXCOMPONENT_MOUSE_CANCEL)) {
      g_mouse_buttons_by_window.erase(surface_window_id);
    }
  }
  const size_t timestamp = static_cast<size_t>(NowMicros());
  ForwardPointer(surface_window_id, phase, me.x, me.y, buttons,
                 kLynxtronMouseDeviceId,
                 /*mouse=*/1, timestamp);
}

void DispatchHoverEvent(OH_NativeXComponent* component, bool isHover) {}

OH_NativeXComponent_MouseEvent_Callback g_mouse_callback = {
    .DispatchMouseEvent = DispatchMouseEvent,
    .DispatchHoverEvent = DispatchHoverEvent,
};

// Mouse-wheel and touchpad scrolling are delivered by ArkUI as AXIS events,
// not through OH_NativeXComponent_MouseEvent. Forward their physical deltas
// as Lynx scroll signals so ScrollView/List can consume them directly.
void DispatchAxisEvent(OH_NativeXComponent* component,
                       ArkUI_UIInputEvent* event,
                       ArkUI_UIInputEvent_Type type) {
  if (!component || !event || type != ARKUI_UIINPUTEVENT_TYPE_AXIS) return;
  const double delta_x = OH_ArkUI_AxisEvent_GetHorizontalAxisValue(event);
  const double delta_y = OH_ArkUI_AxisEvent_GetVerticalAxisValue(event);
  if (delta_x == 0.0 && delta_y == 0.0) return;
  int32_t surface_window_id = GetSurfaceWindowId(component);
  if (surface_window_id <= 0) surface_window_id = g_current_harmony_window_id;
  const int32_t source = OH_ArkUI_UIInputEvent_GetSourceType(event);
  const bool precise = source != 1;
  ForwardScroll(surface_window_id, OH_ArkUI_PointerEvent_GetX(event),
                OH_ArkUI_PointerEvent_GetY(event), delta_x, delta_y, precise,
                static_cast<size_t>(NowMicros()));
}

uint64_t ToLynxLogicalKey(OH_NativeXComponent_KeyCode code) {
  switch (code) {
    case KEY_DEL: return 0x00100000008ULL;
    case KEY_TAB: return 0x00100000009ULL;
    case KEY_ENTER:
    case KEY_NUMPAD_ENTER: return 0x0010000000dULL;
    case KEY_ESCAPE: return 0x0010000001bULL;
    case KEY_SPACE: return 0x20ULL;
    case KEY_FORWARD_DEL: return 0x0010000007fULL;
    case KEY_DPAD_DOWN: return 0x00100000301ULL;
    case KEY_DPAD_LEFT: return 0x00100000302ULL;
    case KEY_DPAD_RIGHT: return 0x00100000303ULL;
    case KEY_DPAD_UP: return 0x00100000304ULL;
    case KEY_MOVE_END: return 0x00100000305ULL;
    case KEY_MOVE_HOME: return 0x00100000306ULL;
    case KEY_PAGE_DOWN: return 0x00100000307ULL;
    case KEY_PAGE_UP: return 0x00100000308ULL;
    case KEY_INSERT: return 0x00100000407ULL;
    case KEY_CTRL_LEFT: return 0x00200000100ULL;
    case KEY_CTRL_RIGHT: return 0x00200000101ULL;
    case KEY_SHIFT_LEFT: return 0x00200000102ULL;
    case KEY_SHIFT_RIGHT: return 0x00200000103ULL;
    case KEY_ALT_LEFT: return 0x00200000104ULL;
    case KEY_ALT_RIGHT: return 0x00200000105ULL;
    case KEY_META_LEFT: return 0x00200000106ULL;
    case KEY_META_RIGHT: return 0x00200000107ULL;
    default:
      if (code >= KEY_A && code <= KEY_Z) return static_cast<uint64_t>('a' + code - KEY_A);
      if (code >= KEY_0 && code <= KEY_9) return static_cast<uint64_t>('0' + code - KEY_0);
      // HarmonyOS reports the numeric keypad independently from the number
      // row.  Preserve the dedicated NumPad logical-key range; Lynx uses it
      // to distinguish keypad navigation from digit input when NumLock is on.
      if (code >= KEY_NUMPAD_0 && code <= KEY_NUMPAD_9) {
        return 0x00200000230ULL +
               static_cast<uint64_t>(code - KEY_NUMPAD_0);
      }
      if (code >= KEY_F1 && code <= KEY_F12) return 0x00100000800ULL + code - KEY_F1 + 1;
      return 0;
  }
}

void DispatchKeyEvent(OH_NativeXComponent* component, void*) {
  OH_NativeXComponent_KeyEvent* key_event = nullptr;
  if (OH_NativeXComponent_GetKeyEvent(component, &key_event) != 0 ||
      !key_event) {
    return;
  }
  OH_NativeXComponent_KeyAction action = OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN;
  OH_NativeXComponent_KeyCode code = KEY_UNKNOWN;
  if (OH_NativeXComponent_GetKeyEventAction(key_event, &action) != 0 ||
      OH_NativeXComponent_GetKeyEventCode(key_event, &code) != 0 ||
      action == OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN) return;
  if (!g_send_key && g_lynxtron_handle) {
    g_send_key = reinterpret_cast<SendKeyFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendKeyEventForWindow"));
  }
  if (!g_send_key) {
    OH_LOG_ERROR(LOG_APP,
                 "[KEY] LynxtronSendKeyEventForWindow unavailable "
                 "handle=%{public}p error=%{public}s",
                 g_lynxtron_handle, dlerror());
  }
  if (g_send_key) {
    int32_t surface_window_id = GetSurfaceWindowId(component);
    if (surface_window_id <= 0) {
      surface_window_id = g_current_harmony_window_id;
    }
    const uint64_t logical = ToLynxLogicalKey(code);
    g_send_key(surface_window_id,
               action == OH_NATIVEXCOMPONENT_KEY_ACTION_UP ? 0 : 1,
               logical, 0, NowMicros());
    // HarmonyOS PC delivers some hardware number-row keys as keypad-style
    // events.  A key event carries no character payload, so Clay cannot turn
    // those events into text even though letters work.  Queue the committed
    // digit after key-down so the focused editor has processed the key first.
    if (action != OH_NATIVEXCOMPONENT_KEY_ACTION_UP &&
        ((code >= KEY_0 && code <= KEY_9) ||
         (code >= KEY_NUMPAD_0 && code <= KEY_NUMPAD_9))) {
      if (!g_send_text && g_lynxtron_handle) {
        g_send_text = reinterpret_cast<SendTextFn>(
            dlsym(g_lynxtron_handle, "LynxtronSendTextInputForWindow"));
      }
      if (g_send_text) {
        char digit[2] = {
            static_cast<char>('0' +
                              ((code >= KEY_NUMPAD_0)
                                   ? code - KEY_NUMPAD_0
                                   : code - KEY_0)),
            '\0'};
        g_send_text(surface_window_id, digit, NowMicros());
      }
    }
  }
}

// NAPI export notifyWindowState(windowId, state, [resizeEdge]): forwards an
// ArkTS window-state report into liblynxtron (LynxtronNotifyWindowState).
napi_value NotifyWindowState(napi_env env, napi_callback_info info) {
  OH_LOG_INFO(LOG_APP, "[WINEVENT] NotifyWindowState called");
  size_t argc = 3;
  napi_value args[3] = {nullptr, nullptr, nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  int32_t window_id = -1;
  char state[64] = {0};
  size_t state_len = 0;
  int32_t resize_edge = kLynxtronResizeEdgeBottomRight;

  if (argc >= 2 &&
      napi_get_value_int32(env, args[0], &window_id) == napi_ok &&
      napi_get_value_string_utf8(env, args[1], state, sizeof(state),
                                 &state_len) == napi_ok) {
    if (argc >= 3) {
      napi_get_value_int32(env, args[2], &resize_edge);
    }

    OH_LOG_INFO(LOG_APP,
                "[WINEVENT] notifyWindowState windowId=%{public}d "
                "state=%{public}s resizeEdge=%{public}d argc=%{public}d",
                window_id, state, resize_edge, (int)argc);

    if (EnsureLynxtronLoaded()) {
      auto fn = reinterpret_cast<LynxtronNotifyWindowStateFn>(
          dlsym(g_lynxtron_handle, "LynxtronNotifyWindowState"));
      if (fn) {
        fn(window_id, state, resize_edge);
      } else {
        OH_LOG_ERROR(LOG_APP,
                     "dlsym LynxtronNotifyWindowState FAILED: %{public}s",
                     dlerror());
      }
    } else {
      OH_LOG_ERROR(LOG_APP,
                   "[WINEVENT] notifyWindowState: liblynxtron.so not loaded");
    }
  } else {
    OH_LOG_ERROR(LOG_APP,
                 "[WINEVENT] notifyWindowState: bad args argc=%{public}d",
                 (int)argc);
    napi_throw_error(env, nullptr,
                     "notifyWindowState requires (windowId, state, [resizeEdge])");
    return nullptr;
  }

  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

static napi_value NotifyWindowRect(napi_env env, napi_callback_info info) {
  size_t argc = 5;
  napi_value args[5] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  int32_t window_id = -1;
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;

  if (argc >= 5 &&
      napi_get_value_int32(env, args[0], &window_id) == napi_ok &&
      napi_get_value_int32(env, args[1], &x) == napi_ok &&
      napi_get_value_int32(env, args[2], &y) == napi_ok &&
      napi_get_value_int32(env, args[3], &width) == napi_ok &&
      napi_get_value_int32(env, args[4], &height) == napi_ok) {
    OH_LOG_INFO(LOG_APP,
                "[WINEVENT] notifyWindowRect windowId=%{public}d "
                "rect=%{public}d,%{public}d,%{public}dx%{public}d",
                window_id, x, y, width, height);

    if (EnsureLynxtronLoaded()) {
      auto fn = reinterpret_cast<LynxtronNotifyWindowRectFn>(
          dlsym(g_lynxtron_handle, "LynxtronNotifyWindowRect"));
      if (fn) {
        fn(window_id, x, y, width, height);
      } else {
        OH_LOG_ERROR(LOG_APP,
                     "dlsym LynxtronNotifyWindowRect FAILED: %{public}s",
                     dlerror());
      }
    } else {
      OH_LOG_ERROR(LOG_APP,
                   "[WINEVENT] notifyWindowRect: liblynxtron.so not loaded");
    }
  } else {
    OH_LOG_ERROR(LOG_APP,
                 "[WINEVENT] notifyWindowRect: bad args argc=%{public}d",
                 (int)argc);
    napi_throw_error(env, nullptr,
                     "notifyWindowRect requires (windowId, x, y, width, height)");
    return nullptr;
  }

  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

// ---------------------------------------------------------------------------
// HarmonyOS native input method client
//
// XComponent draws its own text, so it has no ArkUI text node the system IME
// can bind to. The inputmethod NDK exists for exactly this case: we register an
// InputMethod_TextEditorProxy and become the text client ourselves, the same
// way the Windows TextInputPlugin and the macOS NSTextInputClient do for Clay.
//
// The previous approach — floating a transparent ArkUI TextInput over the
// surface and forwarding its onChange — cannot work on this build: the IME
// leaves characters in the proxy's *preview text* buffer (AceTextField
// SetPreviewText) and onChange never fires, so nothing was ever forwarded. It
// also stole focus from XComponent, which killed physical-key handling.
// ---------------------------------------------------------------------------

std::mutex g_ime_mutex;
InputMethod_TextEditorProxy* g_editor_proxy = nullptr;
InputMethod_InputMethodProxy* g_ime_proxy = nullptr;  // non-null == attached
// Caret rect last reported by Lynx, in surface-local physical pixels.
double g_caret_x = 0, g_caret_y = 0, g_caret_w = 1, g_caret_h = 1;
// Window the editor lives in, published from ETS (see SetWindowId). The input
// method service routes keyboard focus per window, so leaving this unset can
// leave an attached client that never receives text. Atomic because it is read
// from the IME IPC thread and written from the ArkUI main thread.
std::atomic<int32_t> g_window_id{-1};

// Event-driven text-input focus state. The liblynxtron.so renderer invokes
// LynxtronSetHarmonyTextInputFocusCallback() whenever a window gains/loses an
// editable focus or its caret moves. We cache the latest state here and apply
// attach/detach/cursor-update in SyncImeImpl on the ArkUI thread.
std::atomic<int32_t> g_focus_window_id{-1};
std::atomic<bool> g_focus_visible{false};
std::atomic<bool> g_focus_changed{false};
std::atomic<double> g_focus_caret_x{0};
std::atomic<double> g_focus_caret_y{0};
std::atomic<double> g_focus_caret_w{1};
std::atomic<double> g_focus_caret_h{1};

napi_ref g_ability_context_ref = nullptr;
std::string g_last_synced_title;

// Reference to the OHOS window.Window object (the main window), published from
// EntryAbility via setWindowObject. Used to drive window-decor visibility.
napi_ref g_window_ref = nullptr;

// Thread-safe function carrying native window-decor commands from the Lynx UI
// thread to the ArkUI main thread, where the OHOS window.Window verbs run.
// Created in setWindowObject.
napi_threadsafe_function g_window_cmd_tsfn = nullptr;

// Runs on the ArkUI main thread (invoked by the thread-safe function). The
// detached LynxtronMain thread has finished; call terminateSelf() on the stored
// ability context so the framework tears down the ability and the process ends
// gracefully — exit()/_exit() are forbidden on OpenHarmony (appspawn aborts).
void ExitCallJS(napi_env env, napi_value, void*, void* data) {
  int rc = 0;
  if (data) {
    rc = *static_cast<int*>(data);
    delete static_cast<int*>(data);  // owned by the caller (heap-allocated)
  }
  OH_LOG_INFO(LOG_APP, "[Exit] LynxtronMain finished (rc=%{public}d), "
              "calling terminateSelf", rc);
  if (!g_ability_context_ref) {
    OH_LOG_ERROR(LOG_APP, "[Exit] no ability context ref — cannot terminate");
    return;
  }
  napi_value ability_ctx = nullptr;
  if (napi_get_reference_value(env, g_ability_context_ref, &ability_ctx) !=
          napi_ok ||
      !ability_ctx) {
    OH_LOG_ERROR(LOG_APP, "[Exit] cannot resolve ability context");
    return;
  }
  napi_value terminate_self = nullptr;
  if (napi_get_named_property(env, ability_ctx, "terminateSelf",
                              &terminate_self) != napi_ok ||
      !terminate_self) {
    OH_LOG_ERROR(LOG_APP, "[Exit] no terminateSelf on ability context");
    return;
  }
  napi_call_function(env, ability_ctx, terminate_self, 0, nullptr, nullptr);
}

// Creates the thread-safe function once, on the ArkUI main thread (the env it
// binds to). Called from SetAbilityContext after the context ref is stored.
void EnsureExitThreadsafeFunction(napi_env env) {
  if (g_exit_tsfn) return;
  napi_value resource_name = nullptr;
  napi_create_string_utf8(env, "lynxtron-exit", NAPI_AUTO_LENGTH,
                          &resource_name);
  napi_create_threadsafe_function(
      env, /*func=*/nullptr, /*async_resource=*/nullptr, resource_name,
      /*max_queue_size=*/0, /*initial_thread_count=*/1,
      /*thread_finalize_data=*/nullptr, /*thread_finalize_cb=*/nullptr,
      /*context=*/nullptr, ExitCallJS, &g_exit_tsfn);
  OH_LOG_INFO(LOG_APP, "[Exit] threadsafe function created: %{public}p",
              (void*)g_exit_tsfn);
}

void ForwardKey(uint64_t logical) {
  if (!g_send_key && g_lynxtron_handle) {
    g_send_key = reinterpret_cast<SendKeyFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendKeyEventForWindow"));
  }
  if (!g_send_key) return;
  // Clay's editable acts on key-down (or repeat) and ignores the up, but send
  // both so the focus manager's pressed-key bookkeeping stays balanced.
  int32_t window_id = g_window_id.load();
  g_send_key(window_id, 1, logical, 0, NowMicros());
  g_send_key(window_id, 0, logical, 0, NowMicros());
}

// Invoked from the liblynxtron.so renderer whenever this window's editable
// focus or caret changes. Runs on the Lynx/Clay sequence, not the ArkUI thread,
// so we only cache the latest state and let SyncImeImpl apply it on ArkUI.
void OnHarmonyTextInputFocus(int32_t harmony_window_id,
                             bool visible,
                             float x,
                             float y,
                             float width,
                             float height) {
  g_focus_window_id.store(harmony_window_id);
  g_focus_visible.store(visible);
  g_focus_caret_x.store(x);
  g_focus_caret_y.store(y);
  g_focus_caret_w.store(width > 0 ? width : 1);
  g_focus_caret_h.store(height > 0 ? height : 1);
  g_focus_changed.store(true);
  OH_LOG_INFO(LOG_APP,
              "[IME] focus event window=%{public}d visible=%{public}d "
              "caret=%{public}f,%{public}f,%{public}f,%{public}f",
              harmony_window_id, static_cast<int>(visible), x, y, width, height);
}

// The IME hands us UTF-16; Lynx wants UTF-8. Lone surrogates are dropped
// rather than encoded, so a truncated pair can never produce invalid UTF-8.
std::string Utf16ToUtf8(const char16_t* text, size_t length) {
  std::string out;
  out.reserve(length * 3);
  for (size_t i = 0; i < length; ++i) {
    uint32_t cp = text[i];
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (i + 1 >= length) break;
      uint32_t low = text[i + 1];
      if (low < 0xDC00 || low > 0xDFFF) continue;
      cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
      ++i;
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
      continue;  // unpaired low surrogate
    }
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

// ---- InputMethod_TextEditorProxy callbacks (IPC thread) ----
//
// Everything below hands off to liblynxtron.so exports that re-post onto the
// Lynx UI runner, so running on the IME's IPC thread is safe.

void OnImeGetTextConfig(InputMethod_TextEditorProxy*,
                        InputMethod_TextConfig* config) {
  OH_TextConfig_SetInputType(config, IME_TEXT_INPUT_TYPE_TEXT);
  OH_TextConfig_SetEnterKeyType(config, IME_ENTER_KEY_DONE);
  // Match the proven C implementation: leave pre-edit disabled and accept only
  // the IME's final InsertText candidate.  Replaying every pinyin preview into
  // Lynx corrupts the editable value before the Chinese candidate is committed.
  OH_TextConfig_SetPreviewTextSupport(config, false);
  OH_TextConfig_SetSelection(config, 0, 0);

  {
    std::lock_guard<std::mutex> lock(g_ime_mutex);
    int32_t window_id = g_window_id.load();
    if (window_id >= 0) {
      OH_TextConfig_SetWindowId(config, window_id);
    }
    InputMethod_CursorInfo* cursor = nullptr;
    if (OH_TextConfig_GetCursorInfo(config, &cursor) == IME_ERR_OK && cursor) {
      OH_CursorInfo_SetRect(cursor, g_caret_x, g_caret_y, g_caret_w, g_caret_h);
    }
    OH_LOG_INFO(LOG_APP, "[IME] GetTextConfig served (windowId=%{public}d)",
                window_id);
  }
}

// IME InsertText callback (runs on the IME IPC thread): converts the committed
// UTF-16 text to UTF-8 and forwards it to Lynx as text input.
void OnImeInsertText(InputMethod_TextEditorProxy*, const char16_t* text,
                     size_t length) {
  std::string utf8 = Utf16ToUtf8(text, length);
  if (utf8.empty()) return;
  if (!g_send_text && g_lynxtron_handle) {
    g_send_text = reinterpret_cast<SendTextFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendTextInputForWindow"));
  }
  int32_t window_id = g_window_id.load();
  OH_LOG_INFO(LOG_APP, "[IME] InsertText bytes=%{public}zu dispatch=%{public}d window=%{public}d",
              utf8.size(), g_send_text != nullptr, window_id);
  if (g_send_text) g_send_text(window_id, utf8.c_str(), NowMicros());
}

void OnImeDeleteForward(InputMethod_TextEditorProxy*, int32_t length) {
  OH_LOG_INFO(LOG_APP, "[IME] DeleteForward len=%{public}d", length);
  for (int32_t i = 0; i < length; ++i) ForwardKey(kLogicalDelete);
}

void OnImeDeleteBackward(InputMethod_TextEditorProxy*, int32_t length) {
  OH_LOG_INFO(LOG_APP, "[IME] DeleteBackward len=%{public}d", length);
  for (int32_t i = 0; i < length; ++i) ForwardKey(kLogicalBackspace);
}

void OnImeSendKeyboardStatus(InputMethod_TextEditorProxy*,
                             InputMethod_KeyboardStatus status) {
  OH_LOG_INFO(LOG_APP, "[IME] keyboard status=%{public}d", (int)status);
}

void OnImeSendEnterKey(InputMethod_TextEditorProxy*,
                       InputMethod_EnterKeyType) {
  ForwardKey(kLogicalEnter);
}

void OnImeMoveCursor(InputMethod_TextEditorProxy*,
                     InputMethod_Direction direction) {
  switch (direction) {
    case IME_DIRECTION_UP: ForwardKey(kLogicalArrowUp); break;
    case IME_DIRECTION_DOWN: ForwardKey(kLogicalArrowDown); break;
    case IME_DIRECTION_LEFT: ForwardKey(kLogicalArrowLeft); break;
    case IME_DIRECTION_RIGHT: ForwardKey(kLogicalArrowRight); break;
    default: break;
  }
}

void OnImeHandleSetSelection(InputMethod_TextEditorProxy*, int32_t start,
                             int32_t end) {
  OH_LOG_INFO(LOG_APP, "[IME] setSelection %{public}d..%{public}d", start, end);
}

void OnImeHandleExtendAction(InputMethod_TextEditorProxy*,
                             InputMethod_ExtendAction action) {
  OH_LOG_INFO(LOG_APP, "[IME] extendAction=%{public}d", (int)action);
}

// Lynx does not expose the editor's surrounding text to the embedder, so these
// report "nothing available". IMEs treat that as an editor without context and
// fall back to context-free candidates, which is correct if unpolished.
void OnImeGetLeftText(InputMethod_TextEditorProxy*, int32_t, char16_t*,
                      size_t* length) {
  if (length) *length = 0;
}

void OnImeGetRightText(InputMethod_TextEditorProxy*, int32_t, char16_t*,
                       size_t* length) {
  if (length) *length = 0;
}

int32_t OnImeGetTextIndexAtCursor(InputMethod_TextEditorProxy*) { return 0; }

int32_t OnImeReceivePrivateCommand(InputMethod_TextEditorProxy*,
                                   InputMethod_PrivateCommand**, size_t) {
  return IME_ERR_OK;
}

int32_t OnImeSetPreviewText(InputMethod_TextEditorProxy*, const char16_t* text,
                            size_t length, int32_t, int32_t) {
  // Preview support is declared off in GetTextConfig, so this should not be
  // reached; forward it anyway rather than silently dropping input if some IME
  // ignores the flag.
  std::string utf8 = Utf16ToUtf8(text, length);
  if (!g_send_composing_text && g_lynxtron_handle) {
    g_send_composing_text = reinterpret_cast<SendTextFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendComposingTextForWindow"));
  }
  int32_t window_id = g_window_id.load();
  OH_LOG_INFO(LOG_APP, "[IME] SetPreviewText bytes=%{public}zu window=%{public}d",
              utf8.size(), window_id);
  if (g_send_composing_text) g_send_composing_text(window_id, utf8.c_str(), NowMicros());
  return IME_ERR_OK;
}

void OnImeFinishTextPreview(InputMethod_TextEditorProxy*) {
  if (!g_send_composing_text && g_lynxtron_handle) {
    g_send_composing_text = reinterpret_cast<SendTextFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendComposingTextForWindow"));
  }
  int32_t window_id = g_window_id.load();
  OH_LOG_INFO(LOG_APP, "[IME] FinishTextPreview window=%{public}d", window_id);
  if (g_send_composing_text) g_send_composing_text(window_id, "", NowMicros());
}

// Builds the editor proxy once. Returns false if the NDK rejects any callback.
bool EnsureEditorProxy() {
  if (g_editor_proxy) return true;
  g_editor_proxy = OH_TextEditorProxy_Create();
  if (!g_editor_proxy) {
    OH_LOG_ERROR(LOG_APP, "[IME] OH_TextEditorProxy_Create failed");
    return false;
  }
  // Every setter must succeed: Attach rejects a proxy with any callback unset.
  OH_TextEditorProxy_SetGetTextConfigFunc(g_editor_proxy, OnImeGetTextConfig);
  OH_TextEditorProxy_SetInsertTextFunc(g_editor_proxy, OnImeInsertText);
  OH_TextEditorProxy_SetDeleteForwardFunc(g_editor_proxy, OnImeDeleteForward);
  OH_TextEditorProxy_SetDeleteBackwardFunc(g_editor_proxy, OnImeDeleteBackward);
  OH_TextEditorProxy_SetSendKeyboardStatusFunc(g_editor_proxy,
                                               OnImeSendKeyboardStatus);
  OH_TextEditorProxy_SetSendEnterKeyFunc(g_editor_proxy, OnImeSendEnterKey);
  OH_TextEditorProxy_SetMoveCursorFunc(g_editor_proxy, OnImeMoveCursor);
  OH_TextEditorProxy_SetHandleSetSelectionFunc(g_editor_proxy,
                                               OnImeHandleSetSelection);
  OH_TextEditorProxy_SetHandleExtendActionFunc(g_editor_proxy,
                                               OnImeHandleExtendAction);
  OH_TextEditorProxy_SetGetLeftTextOfCursorFunc(g_editor_proxy,
                                                OnImeGetLeftText);
  OH_TextEditorProxy_SetGetRightTextOfCursorFunc(g_editor_proxy,
                                                 OnImeGetRightText);
  OH_TextEditorProxy_SetGetTextIndexAtCursorFunc(g_editor_proxy,
                                                 OnImeGetTextIndexAtCursor);
  OH_TextEditorProxy_SetReceivePrivateCommandFunc(g_editor_proxy,
                                                  OnImeReceivePrivateCommand);
  OH_TextEditorProxy_SetSetPreviewTextFunc(g_editor_proxy, OnImeSetPreviewText);
  OH_TextEditorProxy_SetFinishTextPreviewFunc(g_editor_proxy,
                                              OnImeFinishTextPreview);
  OH_LOG_INFO(LOG_APP, "[IME] editor proxy ready");
  return true;
}

void AttachIme() {
  if (g_ime_proxy || !EnsureEditorProxy()) return;
  InputMethod_AttachOptions* options = OH_AttachOptions_Create(true);
  if (!options) {
    OH_LOG_ERROR(LOG_APP, "[IME] OH_AttachOptions_Create failed");
    return;
  }
  InputMethod_ErrorCode rc =
      OH_InputMethodController_Attach(g_editor_proxy, options, &g_ime_proxy);
  OH_AttachOptions_Destroy(options);
  if (rc != IME_ERR_OK) {
    OH_LOG_ERROR(LOG_APP, "[IME] Attach FAILED rc=%{public}d", (int)rc);
    g_ime_proxy = nullptr;
    return;
  }
  OH_LOG_INFO(LOG_APP, "[IME] attached, keyboard requested");
}

void DetachIme() {
  if (!g_ime_proxy) return;
  InputMethod_ErrorCode rc = OH_InputMethodController_Detach(g_ime_proxy);
  OH_LOG_INFO(LOG_APP, "[IME] detached rc=%{public}d", (int)rc);
  g_ime_proxy = nullptr;
}

// Published from BaseWindowAbility.onWindowStageCreate for each window.
napi_value SetWindowId(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  int32_t id = -1;
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok &&
      argc == 1) {
    napi_get_value_int32(env, argv[0], &id);
  }
  {
    std::lock_guard<std::mutex> lock(g_ime_mutex);
    g_window_id.store(id);
  }
  // Remember which window this Ability belongs to so surface lifecycle callbacks
  // can be routed per-window instead of assuming a single window.
  g_current_harmony_window_id = id;
  // Also route native window operations through this window id. Without this
  // NativeWindowHarmony::InvokeWindowOp looks up callbacks with id=-1 and
  // drops every minimize/restore/show/focus/etc. request.
  if (EnsureLynxtronLoaded()) {
    auto fn = reinterpret_cast<LynxtronSetWindowIdFn>(
        dlsym(g_lynxtron_handle, "LynxtronSetWindowId"));
    if (fn) {
      fn(id);
    } else {
      OH_LOG_ERROR(LOG_APP,
                   "dlsym LynxtronSetWindowId FAILED: %{public}s",
                   dlerror());
    }
  }
  OH_LOG_INFO(LOG_APP, "[IME] windowId=%{public}d", id);
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

// Published from BaseWindowAbility.onWindowStageCreate for each window,
// carrying both the C++-allocated window id and the HarmonyOS origin window id.
napi_value SetWindowIdForWindow(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {};
  int32_t cpp_window_id = -1;
  int32_t harmony_window_id = -1;
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok &&
      argc == 2) {
    napi_get_value_int32(env, argv[0], &cpp_window_id);
    napi_get_value_int32(env, argv[1], &harmony_window_id);
  }
  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] setWindowIdForWindow cpp=%{public}d harmony=%{public}d",
              cpp_window_id, harmony_window_id);
  // Surface lifecycle callbacks on this thread belong to this window.
  g_current_harmony_window_id = harmony_window_id;
  // IME can only focus one window at a time. Since setWindowIdForWindow is
  // called when a window is created/shown, treat the newly bound window as the
  // current IME target so text input reaches the right surface. This keeps the
  // legacy single-window IME behavior intact while multi-window rendering is
  // routed per-window.
  {
    std::lock_guard<std::mutex> lock(g_ime_mutex);
    g_window_id.store(harmony_window_id);
  }
  if (EnsureLynxtronLoaded()) {
    auto fn = reinterpret_cast<void (*)(int32_t, int32_t)>(
        dlsym(g_lynxtron_handle, "LynxtronOnHarmonyWindowCreated"));
    if (fn) {
      fn(cpp_window_id, harmony_window_id);
    } else {
      OH_LOG_ERROR(LOG_APP,
                   "dlsym LynxtronOnHarmonyWindowCreated FAILED: %{public}s",
                   dlerror());
    }
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value SetAbilityContext(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok &&
      argc == 1 && argv[0] != nullptr) {
    if (g_ability_context_ref) {
      napi_delete_reference(env, g_ability_context_ref);
    }
    napi_create_reference(env, argv[0], 1, &g_ability_context_ref);
    OH_LOG_INFO(LOG_APP, "abilityContext stored");
    EnsureExitThreadsafeFunction(env);

    napi_value files_dir = nullptr;
    if (napi_get_named_property(env, argv[0], "filesDir", &files_dir) ==
            napi_ok &&
        files_dir != nullptr) {
      size_t len = 0;
      napi_get_value_string_utf8(env, files_dir, nullptr, 0, &len);
      std::string path(len, '\0');
      napi_get_value_string_utf8(env, files_dir, &path[0], len + 1, &len);
      setenv("LYNXTRON_FILES_DIR", path.c_str(), 1);
      OH_LOG_INFO(LOG_APP, "LYNXTRON_FILES_DIR set to: %{public}s",
                  path.c_str());
    }

    napi_value temp_dir = nullptr;
    if (napi_get_named_property(env, argv[0], "tempDir", &temp_dir) ==
            napi_ok &&
        temp_dir != nullptr) {
      size_t len = 0;
      napi_get_value_string_utf8(env, temp_dir, nullptr, 0, &len);
      std::string path(len, '\0');
      napi_get_value_string_utf8(env, temp_dir, &path[0], len + 1, &len);
      setenv("TMPDIR", path.c_str(), 1);
      OH_LOG_INFO(LOG_APP, "TMPDIR set to: %{public}s", path.c_str());
    }

    napi_value resource_dir = nullptr;
    if (napi_get_named_property(env, argv[0], "resourceDir", &resource_dir) ==
            napi_ok &&
        resource_dir != nullptr) {
      size_t len = 0;
      napi_get_value_string_utf8(env, resource_dir, nullptr, 0, &len);
      std::string path(len, '\0');
      napi_get_value_string_utf8(env, resource_dir, &path[0], len + 1, &len);
      setenv("LYNXTRON_EXE_PATH", path.c_str(), 1);
      OH_LOG_INFO(LOG_APP, "LYNXTRON_EXE_PATH set to: %{public}s",
                  path.c_str());
    }
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value OpenUrl(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok &&
      argc == 1 && argv[0] != nullptr) {
    size_t len = 0;
    napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len);
    std::string url(len, '\0');
    napi_get_value_string_utf8(env, argv[0], &url[0], len + 1, &len);

    if (!g_handle_open_url) {
      EnsureLynxtronLoaded();
      if (g_lynxtron_handle) {
        g_handle_open_url = reinterpret_cast<LynxtronHandleOpenURLFn>(
            dlsym(g_lynxtron_handle, "LynxtronHandleOpenURL"));
      }
    }
    if (g_handle_open_url) {
      g_handle_open_url(url.c_str());
      OH_LOG_INFO(LOG_APP, "openUrl dispatched: %{public}s", url.c_str());
    } else {
      OH_LOG_WARN(LOG_APP, "LynxtronHandleOpenURL symbol not found");
    }
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value OpenPath(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok &&
      argc == 1 && argv[0] != nullptr) {
    size_t len = 0;
    napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len);
    std::string file_path(len, '\0');
    napi_get_value_string_utf8(env, argv[0], &file_path[0], len + 1, &len);

    if (!g_handle_open_path) {
      EnsureLynxtronLoaded();
      if (g_lynxtron_handle) {
        g_handle_open_path = reinterpret_cast<LynxtronHandleOpenPathFn>(
            dlsym(g_lynxtron_handle, "LynxtronHandleOpenPath"));
      }
    }
    if (g_handle_open_path) {
      g_handle_open_path(file_path.c_str());
      OH_LOG_INFO(LOG_APP, "openPath dispatched: %{public}s", file_path.c_str());
    } else {
      OH_LOG_WARN(LOG_APP, "LynxtronHandleOpenPath symbol not found");
    }
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}
// The ArkTS TSFN callback receives the request type, runs the
// @kit.AppGalleryKit API, and reports results back through these functions.

napi_value ResolveCheckAppUpdate(napi_env env, napi_callback_info info) {
  UPDATEMODEL_LOG("ResolveCheckAppUpdate ENTER");
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok && argc >= 1) {
    size_t len = 0;
    napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len);
    if (len > 0) {
      std::string json(len + 1, '\0');
      napi_get_value_string_utf8(env, argv[0], json.data(), json.size(), &len);
      json.resize(len);
      UPDATEMODEL_LOG("ResolveCheckAppUpdate json=%{public}s", json.c_str());
      if (!g_resolve_check && g_lynxtron_handle)
        g_resolve_check = reinterpret_cast<ResolveCheckFn>(
            dlsym(g_lynxtron_handle, "LynxtronResolveCheckAppUpdate"));
      if (g_resolve_check) g_resolve_check(json.c_str());
    }
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value ResolveShowUpdateDialog(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok && argc >= 1) {
    int32_t code = 0;
    napi_get_value_int32(env, argv[0], &code);
    UPDATEMODEL_LOG("ResolveShowUpdateDialog code=%{public}d", code);
    if (!g_resolve_dialog && g_lynxtron_handle)
      g_resolve_dialog = reinterpret_cast<ResolveDialogFn>(
          dlsym(g_lynxtron_handle, "LynxtronResolveShowUpdateDialog"));
    if (g_resolve_dialog) g_resolve_dialog(code);
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value ResolveLoadProduct(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok && argc >= 1) {
    size_t len = 0;
    napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len);
    if (len > 0) {
      std::string json(len + 1, '\0');
      napi_get_value_string_utf8(env, argv[0], json.data(), json.size(), &len);
      json.resize(len);
      UPDATEMODEL_LOG("ResolveLoadProduct json=%{public}s", json.c_str());
      if (!g_resolve_product && g_lynxtron_handle)
        g_resolve_product = reinterpret_cast<ResolveProductFn>(
            dlsym(g_lynxtron_handle, "LynxtronResolveLoadProduct"));
      if (g_resolve_product) g_resolve_product(json.c_str());
    }
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

// Polled from the ArkUI thread. Attaching there keeps GetTextConfig on the
// UI thread, which is what the NDK documents for the config callback.
//
// Primary source of truth: event-driven focus callbacks from the renderer
// (OnHarmonyTextInputFocus). We apply attach/detach/cursor-update when those
// events arrive. The poll remains as a caret-move fallback and to handle any
// lost events.
void SyncImeImpl(napi_env env) {
  if (!g_get_text_input_state && g_lynxtron_handle) {
    g_get_text_input_state = reinterpret_cast<GetTextInputStateFn>(
        dlsym(g_lynxtron_handle, "LynxtronGetTextInputStateForWindow"));
  }

  // 1. Apply any event-driven focus change received from the renderer.
  bool focus_changed = g_focus_changed.exchange(false);
  float x = 0, y = 0, w = 1, h = 1;
  bool visible = false;
  int32_t window_id = g_window_id.load();

  if (focus_changed) {
    window_id = g_focus_window_id.load();
    visible = g_focus_visible.load();
    x = static_cast<float>(g_focus_caret_x.load());
    y = static_cast<float>(g_focus_caret_y.load());
    w = static_cast<float>(g_focus_caret_w.load());
    h = static_cast<float>(g_focus_caret_h.load());
    {
      std::lock_guard<std::mutex> lock(g_ime_mutex);
      g_window_id.store(window_id);
    }
    OH_LOG_INFO(LOG_APP,
                "[IME] applying focus event window=%{public}d visible=%{public}d",
                window_id, static_cast<int>(visible));
  } else {
    // Fallback / caret-move poll for the currently focused window.
    visible = g_get_text_input_state &&
              g_get_text_input_state(window_id, &x, &y, &w, &h);
  }

  bool caret_moved = false;
  {
    std::lock_guard<std::mutex> lock(g_ime_mutex);
    caret_moved = g_caret_x != x || g_caret_y != y || g_caret_w != w ||
                  g_caret_h != h;
    g_caret_x = x;
    g_caret_y = y;
    g_caret_w = w > 0 ? w : 1;
    g_caret_h = h > 0 ? h : 1;
  }

  if (visible && !g_ime_proxy) {
    AttachIme();
  } else if (!visible && g_ime_proxy) {
    DetachIme();
  } else if (visible && g_ime_proxy && caret_moved) {
    // Keeps the candidate window anchored to the caret as it moves.
    InputMethod_CursorInfo* cursor = OH_CursorInfo_Create(x, y, w, h);
    if (cursor) {
      OH_InputMethodProxy_NotifyCursorUpdate(g_ime_proxy, cursor);
      OH_CursorInfo_Destroy(cursor);
    }
  }
}

void SyncWindowTitle(napi_env env) {
  if (!g_ability_context_ref) return;
  if (!g_get_title && g_lynxtron_handle) {
    g_get_title = reinterpret_cast<GetTitleFn>(
        dlsym(g_lynxtron_handle, "LynxtronGetWindowTitle"));
  }
  if (!g_get_title) return;
  const char* title = g_get_title();
  if (!title || !title[0] || title == g_last_synced_title) return;
  g_last_synced_title = title;
  napi_value context = nullptr;
  napi_get_reference_value(env, g_ability_context_ref, &context);
  if (!context) return;
  napi_value setMissionLabel = nullptr;
  napi_get_named_property(env, context, "setMissionLabel", &setMissionLabel);
  napi_value label = nullptr;
  napi_create_string_utf8(env, title, NAPI_AUTO_LENGTH, &label);
  napi_call_function(env, context, setMissionLabel, 1, &label, nullptr);
  OH_LOG_INFO(LOG_APP, "[Title] setMissionLabel dispatched");
}

// Invokes a single-bool-arg method on the stored OHOS window.Window object.
void CallWindowBoolMethod(napi_env env, const char* method, bool value) {
  if (!g_window_ref) return;
  napi_value window = nullptr;
  if (napi_get_reference_value(env, g_window_ref, &window) != napi_ok || !window)
    return;
  napi_value fn = nullptr;
  if (napi_get_named_property(env, window, method, &fn) != napi_ok || !fn)
    return;
  napi_value arg = nullptr;
  napi_get_boolean(env, value, &arg);
  napi_call_function(env, window, fn, 1, &arg, nullptr);
}

// setWindowTitleButtonVisible(isMaximizeButtonVisible, isMinimizeButtonVisible,
// isCloseButtonVisible).
void CallWindowTitleButtonVisible(napi_env env, bool max, bool min, bool close) {
  if (!g_window_ref) return;
  napi_value window = nullptr;
  if (napi_get_reference_value(env, g_window_ref, &window) != napi_ok || !window)
    return;
  napi_value fn = nullptr;
  if (napi_get_named_property(env, window, "setWindowTitleButtonVisible", &fn) !=
          napi_ok ||
      !fn)
    return;
  napi_value args[3];
  napi_get_boolean(env, max, &args[0]);
  napi_get_boolean(env, min, &args[1]);
  napi_get_boolean(env, close, &args[2]);
  napi_call_function(env, window, fn, 3, args, nullptr);
}

// Runs on the ArkUI main thread (the env the tsfn was created on). Maps the
// native decor command to the OHOS window.Window verbs.
void WindowCommandCallJS(napi_env env, napi_value, void*, void* data) {
  int cmd = data ? *static_cast<int*>(data) : 0;
  delete static_cast<int*>(data);
  bool visible;
  switch (cmd) {
    case 1:  // kWinCmdShowDecor
      visible = true;
      break;
    case 2:  // kWinCmdHideDecor
      visible = false;
      break;
    default:
      return;
  }
  CallWindowBoolMethod(env, "setWindowDecorVisible", visible);
  CallWindowTitleButtonVisible(env, visible, visible, visible);
  OH_LOG_INFO(LOG_APP, "[Window] decor visibility %{public}d dispatched", cmd);
}

// Handler registered into liblynxtron.so via LynxtronSetWindowCommandHandler.
// Called on the Lynx UI thread; posts the command to the ArkUI main thread.
void WindowCommandFromNative(int cmd) {
  if (!g_window_cmd_tsfn) return;
  int* pcmd = new int(cmd);
  napi_call_threadsafe_function(g_window_cmd_tsfn, pcmd, napi_tsfn_nonblocking);
}

// Registers WindowCommandFromNative into liblynxtron.so. Idempotent; no-op
// until the main library is dlopen'd. Called from both setWindowObject and
// start to cover either order.
void RegisterWindowCommandHandler() {
  if (g_set_window_command_handler) return;
  if (!g_lynxtron_handle) return;
  g_set_window_command_handler = reinterpret_cast<SetWindowCommandHandlerFn>(
      dlsym(g_lynxtron_handle, "LynxtronSetWindowCommandHandler"));
  if (g_set_window_command_handler) {
    g_set_window_command_handler(WindowCommandFromNative);
    OH_LOG_INFO(LOG_APP, "[Window] command handler registered");
  } else {
    OH_LOG_ERROR(LOG_APP,
                 "[Window] LynxtronSetWindowCommandHandler not found: %{public}s",
                 dlerror());
  }
}

napi_value SetWindowObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) == napi_ok &&
      argc == 1 && argv[0] != nullptr) {
    if (g_window_ref) {
      napi_delete_reference(env, g_window_ref);
    }
    napi_create_reference(env, argv[0], 1, &g_window_ref);
    OH_LOG_INFO(LOG_APP, "[Window] window object stored");

    // Create the tsfn once so native window-decor commands can be posted
    // straight to the ArkUI main thread (no polling).
    if (!g_window_cmd_tsfn) {
      napi_value resource_name = nullptr;
      napi_create_string_utf8(env, "lynxtron-window-cmd", NAPI_AUTO_LENGTH,
                              &resource_name);
      napi_create_threadsafe_function(
          env, /*func=*/nullptr, /*async_resource=*/nullptr, resource_name,
          /*max_queue_size=*/0, /*initial_thread_count=*/1,
          /*thread_finalize_data=*/nullptr, /*thread_finalize_cb=*/nullptr,
          /*context=*/nullptr, WindowCommandCallJS, &g_window_cmd_tsfn);
      OH_LOG_INFO(LOG_APP, "[Window] command tsfn created: %{public}p",
                  (void*)g_window_cmd_tsfn);
    }

    RegisterWindowCommandHandler();
  }
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value SyncIme(napi_env env, napi_callback_info) {
  SyncImeImpl(env);
  napi_value result = nullptr;
  napi_get_boolean(env, g_ime_proxy != nullptr, &result);
  return result;
}

napi_value SendText(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 1) {
    napi_throw_type_error(env, nullptr, "sendText requires one UTF-8 string");
    return nullptr;
  }
  size_t length = 0;
  if (napi_get_value_string_utf8(env, argv[0], nullptr, 0, &length) != napi_ok || !length) {
    napi_value result = nullptr; napi_get_undefined(env, &result); return result;
  }
  // Reserve the NUL slot requested by N-API. Writing that terminator into a
  // string whose logical size is only |length| is not guaranteed to be safe.
  std::string text(length + 1, '\0');
  napi_get_value_string_utf8(env, argv[0], text.data(), text.size(), &length);
  text.resize(length);
  if (!g_send_text && g_lynxtron_handle) {
    g_send_text = reinterpret_cast<SendTextFn>(
        dlsym(g_lynxtron_handle, "LynxtronSendTextInputForWindow"));
  }
  int32_t window_id = g_window_id.load();
  OH_LOG_INFO(LOG_APP, "[IME] committed text bytes=%{public}zu dispatch=%{public}d window=%{public}d",
              text.size(), g_send_text != nullptr, window_id);
  if (g_send_text) g_send_text(window_id, text.c_str(), NowMicros());
  napi_value result = nullptr; napi_get_undefined(env, &result); return result;
}

napi_value GetTextInputState(napi_env env, napi_callback_info) {
  if (!g_get_text_input_state && g_lynxtron_handle) {
    g_get_text_input_state = reinterpret_cast<GetTextInputStateFn>(
        dlsym(g_lynxtron_handle, "LynxtronGetTextInputStateForWindow"));
  }
  float x = 0, y = 0, width = 1, height = 1;
  int32_t window_id = g_window_id.load();
  bool visible = g_get_text_input_state &&
                 g_get_text_input_state(window_id, &x, &y, &width, &height);
  napi_value result = nullptr; napi_create_object(env, &result);
  napi_value value = nullptr;
  napi_get_boolean(env, visible, &value); napi_set_named_property(env, result, "visible", value);
  napi_create_double(env, x, &value); napi_set_named_property(env, result, "x", value);
  napi_create_double(env, y, &value); napi_set_named_property(env, result, "y", value);
  napi_create_double(env, width, &value); napi_set_named_property(env, result, "width", value);
  napi_create_double(env, height, &value); napi_set_named_property(env, result, "height", value);
  return result;
}

using PowerMonitorNotifyFn = void (*)();

napi_value NotifyPowerMonitor(napi_env env, const char* symbol) {
  if (!EnsureLynxtronLoaded()) {
    napi_throw_error(env, nullptr, "Failed to load liblynxtron.so");
    return nullptr;
  }
  auto notify = reinterpret_cast<PowerMonitorNotifyFn>(
      dlsym(g_lynxtron_handle, symbol));
  if (!notify) {
    OH_LOG_ERROR(LOG_APP, "[PowerMonitor] dlsym %{public}s failed: %{public}s",
                 symbol, dlerror());
    napi_throw_error(env, nullptr, "powerMonitor notify symbol not found");
    return nullptr;
  }
  notify();
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value NotifyPowerMonitorLockScreen(napi_env env, napi_callback_info) {
  return NotifyPowerMonitor(env, "LynxtronPowerMonitorNotifyLockScreen");
}

napi_value NotifyPowerMonitorUnlockScreen(napi_env env, napi_callback_info) {
  return NotifyPowerMonitor(env, "LynxtronPowerMonitorNotifyUnlockScreen");
}

napi_value GetWindowTitle(napi_env env, napi_callback_info) {
  if (!g_get_title && g_lynxtron_handle) {
    g_get_title = reinterpret_cast<GetTitleFn>(
        dlsym(g_lynxtron_handle, "LynxtronGetWindowTitle"));
  }
  const char* title = g_get_title ? g_get_title() : "";
  napi_value result = nullptr;
  napi_create_string_utf8(env, title, NAPI_AUTO_LENGTH, &result);
  return result;
}

// NAPI module init: exports the bridge methods and registers the XComponent
// surface and input callbacks.
napi_value Init(napi_env env, napi_value exports) {
  OH_LOG_INFO(LOG_APP, "Init() called by OHOS framework");

  napi_property_descriptor desc[] = {
      {"start", nullptr, Start, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"registerOpenExternal", nullptr, RegisterOpenExternal, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"registerOpenPath", nullptr, RegisterOpenPath, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"registerShowOpenDialog", nullptr, RegisterShowOpenDialog, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"resolveShowOpenDialog", nullptr, ResolveShowOpenDialog, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"registerShowSaveDialog", nullptr, RegisterShowSaveDialog, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"resolveShowSaveDialog", nullptr, ResolveShowSaveDialog, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"quit", nullptr, Quit, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"sendText", nullptr, SendText, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"getTextInputState", nullptr, GetTextInputState, nullptr, nullptr,
       nullptr, napi_default, nullptr},
      {"syncIme", nullptr, SyncIme, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"setWindowId", nullptr, SetWindowId, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"setWindowIdForWindow", nullptr, SetWindowIdForWindow, nullptr, nullptr,
       nullptr, napi_default, nullptr},
      {"getWindowId", nullptr, GetWindowId, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"registerWindowOpCallback", nullptr, RegisterWindowOpCallback, nullptr,
       nullptr, nullptr, napi_default, nullptr},
      {"registerWindowOpCallbackForWindow", nullptr, RegisterWindowOpCallback,
       nullptr, nullptr, nullptr, napi_default, nullptr},
      {"registerCreateWindowCallback", nullptr, RegisterCreateWindowCallback,
       nullptr, nullptr, nullptr, napi_default, nullptr},
      {"notifyPowerMonitorLockScreen", nullptr, NotifyPowerMonitorLockScreen,
       nullptr, nullptr, nullptr, napi_default, nullptr},
      {"notifyPowerMonitorUnlockScreen", nullptr,
       NotifyPowerMonitorUnlockScreen, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setWindowObject", nullptr, SetWindowObject, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"getWindowTitle", nullptr, GetWindowTitle, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"setAbilityContext", nullptr, SetAbilityContext, nullptr, nullptr,
       nullptr, napi_default, nullptr},
      {"openUrl", nullptr, OpenUrl, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"openPath", nullptr, OpenPath, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"resolveCheckAppUpdate", nullptr, ResolveCheckAppUpdate,
       nullptr, nullptr, nullptr, napi_default, nullptr},
      {"resolveShowUpdateDialog", nullptr, ResolveShowUpdateDialog,
       nullptr, nullptr, nullptr, napi_default, nullptr},
      {"resolveLoadProduct", nullptr, ResolveLoadProduct,
       nullptr, nullptr, nullptr, napi_default, nullptr},
	   {"registerUpdateTSFN", nullptr, RegisterUpdateTSFN,
       nullptr, nullptr, nullptr, napi_default, nullptr},
      {"notifyWindowState", nullptr, NotifyWindowState, nullptr, nullptr,
       nullptr, napi_default, nullptr},
      {"notifyWindowRect", nullptr, NotifyWindowRect, nullptr, nullptr,
       nullptr, napi_default, nullptr},
  };
  napi_status status = napi_define_properties(
      env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  OH_LOG_INFO(LOG_APP, "napi_define_properties status=%{public}d",
              (int)status);

  // Pull the OH_NativeXComponent* off the exports object and register our
  // surface lifecycle callbacks.
  napi_value xc_value = nullptr;
  if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ,
                             &xc_value) == napi_ok &&
      xc_value != nullptr) {
    OH_NativeXComponent* xc = nullptr;
    if (napi_unwrap(env, xc_value, reinterpret_cast<void**>(&xc)) == napi_ok &&
        xc != nullptr) {
      int32_t r = OH_NativeXComponent_RegisterCallback(xc, &g_xc_callback);
      OH_LOG_INFO(LOG_APP, "[XC] RegisterCallback ret=%{public}d", r);
      int32_t rm =
          OH_NativeXComponent_RegisterMouseEventCallback(xc, &g_mouse_callback);
      OH_LOG_INFO(LOG_APP, "[XC] RegisterMouseEventCallback ret=%{public}d", rm);
      int32_t ra = OH_NativeXComponent_RegisterUIInputEventCallback(
          xc, DispatchAxisEvent, ARKUI_UIINPUTEVENT_TYPE_AXIS);
      OH_LOG_INFO(LOG_APP, "[XC] RegisterAxisEventCallback ret=%{public}d", ra);
      int32_t rk = OH_NativeXComponent_RegisterKeyEventCallback(xc, DispatchKeyEvent);
      OH_LOG_INFO(LOG_APP, "[XC] RegisterKeyEventCallback ret=%{public}d", rk);
    } else {
      OH_LOG_ERROR(LOG_APP, "[XC] napi_unwrap xcomponent failed");
    }
  } else {
    OH_LOG_ERROR(LOG_APP, "[XC] no OH_NATIVE_XCOMPONENT_OBJ in exports");
  }

  return exports;
}

}  // namespace

napi_module lynxtron_napi_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "lynxtron_napi",
    .nm_priv = nullptr,
    .reserved = {nullptr},
};

extern "C" __attribute__((constructor)) void RegisterLynxtronNapiModule() {
  OH_LOG_INFO(LOG_APP, "Bridge constructor — registering NAPI module");
  napi_module_register(&lynxtron_napi_module);
  OH_LOG_INFO(LOG_APP, "Bridge constructor done");
}
