// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_API_UPDATE_CHECK_H_
#define LYNXTRON_SHELL_API_API_UPDATE_CHECK_H_

// Cross-library symbols exported from liblynxtron.so and consumed by the
// NAPI bridge (liblynxtron_napi.so) via dlsym. These use extern "C" linkage
// so dlsym can find them by unmangled name.
//
// Communication model (TSFN instead of polling):
//   1. ArkTS init: lynxtron.registerUpdateTSFN(callback)
//      → NAPI bridge creates TSFN → dlsym LynxtronRegisterUpdateTSFN(env, tsfn)
//   2. Node.js request: checkAppUpdate() → DispatchTSFN(kCheckAppUpdate)
//      → napi_call_threadsafe_function(tsfn, &type)
//   3. ArkTS callback: type=0 → updateManager.checkAppUpdate()
//      → lynxtron.resolveCheckAppUpdate(json)
//   4. C++ resolve: dlsym → PostTask(Node.js thread) → Promise resolve

extern "C" {

// ---- TSFN registration (replaces polling consume functions) ----
// Called once by the NAPI bridge during ArkTS initialization.
// |env| and |tsfn| are void* to avoid header conflicts between
// Node.js napi and OHOS NAPI type definitions.
__attribute__((visibility("default")))
void LynxtronRegisterUpdateTSFN(void* env, void* tsfn);

// ---- checkAppUpdate ----
// Called by the NAPI bridge with the result from ArkTS's
// updateManager.checkAppUpdate(). |json| is JSON-serialized CheckUpdateResult.
__attribute__((visibility("default")))
void LynxtronResolveCheckAppUpdate(const char* json);

// ---- showUpdateDialog ----
// Called by the NAPI bridge with the result code from
// updateManager.showUpdateDialog().
__attribute__((visibility("default")))
void LynxtronResolveShowUpdateDialog(int result_code);

// ---- loadProduct ----
// Called by the NAPI bridge with the result from productViewManager.loadProduct().
// |json| is JSON: { success: bool, errorCode?: number, errorMessage?: string }
__attribute__((visibility("default")))
void LynxtronResolveLoadProduct(const char* json);

}  // extern "C"

#endif  // LYNXTRON_SHELL_API_API_UPDATE_CHECK_H_
