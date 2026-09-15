// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/lynx_bringup_harmony.h"

#include <hilog/log.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "shell/api/lynx_view/lynx_view.h"
#include "shell/api/lynx_view/lynx_view_builder.h"
#include "shell/common/global_thread.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "LynxtronBringup"
#define BU_LOG(fmt, ...) OH_LOG_INFO(LOG_APP, "[Bringup] " fmt, ##__VA_ARGS__)
#define BU_ERR(fmt, ...) OH_LOG_ERROR(LOG_APP, "[Bringup] " fmt, ##__VA_ARGS__)

namespace lynxtron {

namespace {

// Owns the bring-up LynxView. Only touched on the UI thread.
std::unique_ptr<LynxView>& BringupView() {
  static std::unique_ptr<LynxView> view;
  return view;
}

// Holds the currently focused LynxView for HarmonyOS input routing. Only
// touched on the UI thread. In a multi-window setup this tracks the focused
// window, not the last-created window.
LynxView*& ActiveView() {
  static LynxView* view = nullptr;
  return view;
}

// Runs on the UI thread: builds a LynxView (attaches the GLDirect windowless
// renderer via the harmony branch of LynxViewBuilder::Build) and loads the
// staged test bundle.
void DoBringup(void* window, int width, int height) {
  if (BringupView()) {
    return;  // already built
  }

  base::FilePath assets;
  if (!base::PathService::Get(base::DIR_ASSETS, &assets)) {
    BU_ERR("DIR_ASSETS unavailable; cannot locate bundle");
    return;
  }
  base::FilePath bundle =
      assets.AppendASCII("resources").AppendASCII("main.lynx.bundle");
  if (!base::PathExists(bundle)) {
    BU_ERR("bundle not found: %{public}s", bundle.value().c_str());
    return;
  }
  BU_LOG("bundle: %{public}s (%{public}dx%{public}d)", bundle.value().c_str(),
         width, height);

  LynxViewBuilder builder;
  builder
      .SetScreenSize(static_cast<float>(width), static_cast<float>(height),
                     1.0f)
      .SetFrame(0, 0, static_cast<float>(width), static_cast<float>(height))
      .SetParent(window);

  auto view = builder.Build();
  if (!view) {
    BU_ERR("LynxViewBuilder::Build() returned null");
    return;
  }
  BU_LOG("LynxView built; loading bundle...");
  view->LoadFile(bundle.AsUTF8Unsafe(), "{}", "{}");
  BringupView() = std::move(view);
  BU_LOG("LoadFile issued; waiting for Clay to composite onto the surface");
}

}  // namespace

// Starts the standalone Lynx bring-up exactly once: waits for Lynxtron's UI
// thread, builds the LynxView and loads the bundled main.lynx bundle onto it.
void LynxtronStartLynxBringup(void* window, int width, int height) {
  static std::once_flag once;
  std::call_once(once, [window, width, height] {
    // The XComponent surface arrives (ETS onLoad) before LynxtronMain has set
    // up the UI thread, so GetUIThreadTaskRunner() is often null here. Poll on
    // a detached thread until the UI thread exists, then post the bring-up onto
    // it. Cap the wait so we don't spin forever if init fails.
    std::thread([window, width, height] {
      for (int i = 0; i < 600; ++i) {  // up to ~60s
        if (auto runner = GetUIThreadTaskRunner()) {
          BU_LOG("UI thread ready after %{public}d ms; posting bring-up",
                 i * 100);
          runner->PostTask(FROM_HERE,
                           base::BindOnce(&DoBringup, window, width, height));
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      BU_ERR("UI thread task runner never became ready; bring-up aborted");
    }).detach();
  });
}

// Sets the LynxView that receives HarmonyOS input routing; called when a
// window gains (non-null) or loses (null) focus.
void SetHarmonyActiveLynxView(LynxView* view) {
  ActiveView() = view;
}

// Delivers a tap on the XComponent surface to the active LynxView as a Lynx
// touch event.
void DispatchHarmonyLynxTouch(int phase, double x, double y, int32_t id) {
  LynxView* view = ActiveView();
  if (!view) {
    BU_ERR("touch dropped: no active LynxView");
    return;
  }
  if (phase != 1) {
    return;
  }
  const int tag =
      view->GetNodeForLocation(static_cast<int>(x), static_cast<int>(y));
  if (tag <= 0) {
    BU_ERR("tap dropped: no node at %{public}f,%{public}f", x, y);
    return;
  }
  BU_LOG("tap tag=%{public}d x=%{public}f y=%{public}f", tag, x, y);
  view->SendTouchEvent("tap", tag, static_cast<float>(x),
                       static_cast<float>(y), static_cast<float>(x),
                       static_cast<float>(y), static_cast<float>(x),
                       static_cast<float>(y));
}

}  // namespace lynxtron
