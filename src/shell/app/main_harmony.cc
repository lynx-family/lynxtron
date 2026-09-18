// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// HarmonyOS entry for the BIG liblynxtron.so (chromium + V8 + Node + Lynx).
//
// This .so bundles Node.js which exports its own napi_* symbols. Trying to
// register a NAPI module here doesn't work because the OHOS framework can't
// reliably resolve the registration with the conflicting symbol table.
//
// Solution (mirrors Electron OHOS port @ chromium114-electron): split into
//   - liblynxtron.so      — this file, big main library, exports LynxtronMain
//   - liblynxtron_napi.so — small bridge in lynxtron_napi_bridge.cc, the .so
//                           that ETS actually imports. It dlopen()s
//                           liblynxtron.so and forwards start() to LynxtronMain.

#include "shell/app/library_main.h"

int main(int argc, char* argv[]) {
  return LynxtronMain(argc, argv);
}
