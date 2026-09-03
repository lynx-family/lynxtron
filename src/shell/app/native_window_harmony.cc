// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/native_window.h"

#include <mutex>

#include "base/memory/weak_ptr.h"
#include "shell/app/application.h"
#include "base/task/single_thread_task_runner.h"
#include <hilog/log.h>

#include <atomic>
#include <string>

#include "base/functional/bind.h"
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "shell/app/native_window_harmony.h"
#include "shell/app/lynx_windowless_renderer_harmony.h"
#include "shell/app/window_creation_bridge_harmony.h"
#include "shell/app/window_list.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/options_switches.h"
#include "shell/ui/gfx/geometry/rect.h"
#include "shell/ui/gfx/geometry/resize_utils.h"
#include "shell/common/global_thread.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "LynxtronWindow"

namespace lynxtron {

// Plain C enum matching gfx::ResizeEdge in
// shell/ui/gfx/geometry/resize_utils.h. liblynxtron.so and the NAPI bridge do
// not share headers, so the values are duplicated here on purpose.
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

static std::string g_harmony_window_title;

// Window-decor commands dispatched to the NAPI bridge's handler (registered via
// LynxtronSetWindowCommandHandler). The handler posts them to the ArkUI main
// thread through a thread-safe function and invokes the matching OHOS
// window.Window verb there. Keep in sync with the bridge switch.
enum HarmonyWindowCommand {
  kWinCmdShowDecor = 1,  // window.setWindowDecorVisible(true) + show the three-button bar
  kWinCmdHideDecor = 2,  // window.setWindowDecorVisible(false) + hide the three-button bar
};

using WindowCommandHandler = void (*)(int);
static WindowCommandHandler g_window_command_handler = nullptr;

// Routes a window-decor command to the NAPI bridge handler registered via
// LynxtronSetWindowCommandHandler.
static void DispatchWindowCommand(int cmd) {
  if (g_window_command_handler) {
    g_window_command_handler(cmd);
  }
}

class NativeWindowHarmony;

namespace {

std::mutex g_window_op_mutex;
std::mutex g_harmony_window_mutex;
base::WeakPtr<NativeWindowHarmony> g_harmony_window;

using WindowOpCallback = void (*)(int32_t window_id, const char* op, const char* args);
std::unordered_map<int32_t, WindowOpCallback> g_window_op_callbacks;

// C++-allocated window id and per-instance lookup table.
static std::atomic<int32_t> g_next_window_id{1};
static std::unordered_map<int32_t, NativeWindowHarmony*> g_id_to_window;
static std::unordered_map<int32_t, NativeWindowHarmony*> g_harmony_id_to_window;
static std::mutex g_window_map_mutex;

// Legacy path: the first EntryAbility window calls LynxtronSetWindowId before
// NativeWindowHarmony is constructed. Cache the id here and let the first
// NativeWindowHarmony consume it.
static std::optional<int32_t> g_pending_legacy_window_id;

// Invokes the ArkTS window-op callback registered for window_id with the given
// verb (e.g. "minimize") and optional string argument.
void InvokeWindowOp(int32_t window_id, const char* op, const char* args = nullptr) {
  if (!op) return;
  OH_LOG_INFO(LOG_APP, "[LynxtronWindow] InvokeWindowOp %{public}s args=%{public}s id=%{public}d",
              op, args ? args : "(null)", window_id);
  std::lock_guard<std::mutex> lock(g_window_op_mutex);
  auto it = g_window_op_callbacks.find(window_id);
  if (it != g_window_op_callbacks.end() && it->second) {
    it->second(window_id, op, args ? args : "");
  } else {
    OH_LOG_WARN(LOG_APP, "[LynxtronWindow] no callback registered for id=%{public}d", window_id);
  }
}

}  // namespace

// Forward declarations; definitions are placed after NativeWindowHarmony so they
// can access the class members for per-instance binding and state notification.
extern "C" __attribute__((visibility("default"))) void LynxtronSetWindowId(int32_t id);
extern "C" __attribute__((visibility("default"))) void LynxtronRegisterWindowOpCallbackForWindow(
    int32_t window_id,
    WindowOpCallback callback);
extern "C" __attribute__((visibility("default"))) int32_t LynxtronGetWindowId();

// Called from lynxtron_napi_bridge.cc when the ArkTS side has finished creating
// the HarmonyOS window for a C++-allocated window id. This binds the two ids
// together so subsequent window operations can be routed per-window.
extern "C" __attribute__((visibility("default"))) void LynxtronOnHarmonyWindowCreated(
    int32_t cpp_window_id,
    int32_t harmony_window_id);
extern "C" __attribute__((visibility("default"))) void LynxtronNotifyWindowState(
    int32_t harmony_window_id,
    const char* state,
    int32_t resize_edge);
extern "C" __attribute__((visibility("default"))) void LynxtronNotifyWindowRect(
    int32_t harmony_window_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height);

// HarmonyOS NativeWindow — minimal concrete implementation.
//
// The window lifecycle and observer bookkeeping live in the NativeWindow base
// class (AddObserver/RemoveObserver, Notify*, InitFromOptions). Previously
// NativeWindow::Create returned nullptr, so JS code doing `new BaseWindow()`
// got a null and crashed at AddObserver. This class provides a real object so
// the JS window API works; actual on-screen rendering is wired later when the
// HAP passes an XComponent surface down (window-render wave).
//
// Bounds/title/visibility are tracked in-process so JS getters return sane
// values. GetNativeWindowHandle() returns the surface pointer once set.
class NativeWindowHarmony : public NativeWindow {
 public:
  NativeWindowHarmony(const gin_helper::Dictionary& options,
                      NativeWindow* parent)
      : NativeWindow(options, parent) {
    // Size the window to the real XComponent surface (physical px) so the
    // LynxView / Clay viewport fills the whole window instead of a fixed
    // 800x600 default (which left content in the bottom-left and made pointer
    // coordinates mismatch). Falls back to 800x600 if the surface has not
    // arrived yet.
    int w = 800, h = 600;
    options.Get(options::kWidth, &w);
    options.Get(options::kHeight, &h);
    if (w <= 0) w = 800;
    if (h <= 0) h = 600;

    int x = 0, y = 0;
    options.Get(options::kX, &x);
    options.Get(options::kY, &y);

    bool show = true;
    options.Get(options::kShow, &show);

    std::string title = Application::Get()->GetName();
    options.Get(options::kTitle, &title);

    bool resizable = true;
    bool movable = true;
    bool minimizable = true;
    bool maximizable = true;
    bool closable = true;
    bool focusable = true;
    options.Get(options::kResizable, &resizable);
    options.Get(options::kMovable, &movable);
    options.Get(options::kMinimizable, &minimizable);
    options.Get(options::kMaximizable, &maximizable);
    options.Get(options::kClosable, &closable);
    options.Get(options::kFocusable, &focusable);

    bool always_on_top = false;
    options.Get(options::kAlwaysOnTop, &always_on_top);

    bool fullscreen = false;
    options.Get(options::kFullscreen, &fullscreen);

    int min_width = 0;
    int min_height = 0;
    int max_width = 0;
    int max_height = 0;
    options.Get(options::kMinWidth, &min_width);
    options.Get(options::kMinHeight, &min_height);
    options.Get(options::kMaxWidth, &max_width);
    options.Get(options::kMaxHeight, &max_height);

    bool modal = false;
    options.Get("modal", &modal);

    int display_id = -1;
    options.Get(options::kDisplayId, &display_id);

    std::string type = "main";
    options.Get(options::kType, &type);

    const bool has_x = options.Has(options::kX);
    const bool has_y = options.Has(options::kY);
    const bool center = options.ValueOrDefault(options::kCenter, !has_x && !has_y);

    // True when this NativeWindowHarmony was bound to an already-created
    // HarmonyOS window through the legacy pending id path (EntryAbility created
    // the first WindowAbility before LynxtronMain ran). In that case the OS
    // window already exists and must NOT be re-created by
    // LynxtronCreateHarmonyWindow, otherwise a spurious second window appears
    // and the first window's binding gets overwritten.
    bool legacy_bound = false;

    {
      std::lock_guard<std::mutex> lock(g_window_map_mutex);
      window_id_ = g_next_window_id.fetch_add(1);
      g_id_to_window[window_id_] = this;
      // The HarmonyOS window id is published later by ArkTS via
      // LynxtronOnHarmonyWindowCreated (or the legacy LynxtronSetWindowId path
      // for the first EntryAbility window). Do not reuse any global id here;
      // each NativeWindowHarmony must be bound explicitly.

      // Consume any pending legacy id published before this NativeWindowHarmony
      // was constructed (only the first window can legitimately consume it).
      if (g_pending_legacy_window_id.has_value() &&
          harmony_window_id_ <= 0) {
        harmony_window_id_ = g_pending_legacy_window_id.value();
        g_harmony_id_to_window[harmony_window_id_] = this;
        g_pending_legacy_window_id.reset();
        legacy_bound = true;
        OH_LOG_INFO(LOG_APP,
                    "[LynxtronWindow] consumed pending legacy harmony id=%{public}d "
                    "for window_id=%{public}d",
                    harmony_window_id_, window_id_);
      }
    }

    OH_LOG_INFO(LOG_APP,
                "[Window] NativeWindowHarmony created %{public}d,%{public}d "
                "%{public}dx%{public}d window_id=%{public}d harmony_id=%{public}d",
                x, y, w, h, window_id_, harmony_window_id_);
    bounds_ = gfx::Rect(x, y, w, h);
    window_bounds_ = bounds_;

    // If this window's XComponent surface has already arrived, size the window
    // to it instead of the requested dimensions. This keeps LynxView's viewport
    // in sync with the actual render target. Look the size up by this window's
    // own harmony id: the legacy global fallback would give a newly created
    // window the previous window's surface size in multi-window mode.
    int surf_w = 0, surf_h = 0;
    bool have_surface_size = false;
    if (harmony_window_id_ > 0) {
      have_surface_size = GetHarmonySurfaceSizeForWindow(harmony_window_id_,
                                                         &surf_w, &surf_h);
    }
    if (have_surface_size && surf_w > 0 && surf_h > 0) {
      bounds_.set_size(gfx::Size(surf_w, surf_h));
      OH_LOG_INFO(LOG_APP,
                  "[Window] sized to existing surface %{public}dx%{public}d",
                  surf_w, surf_h);
    }

    // Resolve parent window ids if a parent NativeWindow was provided.
    NativeWindowHarmony* parent_harmony = nullptr;
    int32_t parent_harmony_id = -1;
    if (parent) {
      parent_harmony = static_cast<NativeWindowHarmony*>(parent);
      parent_harmony_id = parent_harmony->harmony_window_id_;
    }

    // Request ArkTS to create the real HarmonyOS window. The window is
    // considered "created" in-process immediately; the actual Ability will
    // finish asynchronously and call back via LynxtronOnHarmonyWindowCreated.
    // Skip this when the legacy pending id already bound us to an existing
    // window (the first EntryAbility-launched window).
    if (!legacy_bound) {
      HarmonyWindowCreationOptions create_options;
      create_options.x = x;
      create_options.y = y;
      create_options.width = w;
      create_options.height = h;
      create_options.show = show;
      create_options.resizable = resizable;
      create_options.movable = movable;
      create_options.minimizable = minimizable;
      create_options.maximizable = maximizable;
      create_options.closable = closable;
      create_options.focusable = focusable;
      create_options.always_on_top = always_on_top;
      create_options.type = type;
      create_options.center = center;
      create_options.has_x = has_x;
      create_options.has_y = has_y;
      create_options.parent_window_id = parent ? parent_harmony->window_id() : -1;
      create_options.parent_harmony_window_id = parent_harmony_id;
      create_options.title = title;
      create_options.fullscreen = fullscreen;
      create_options.min_width = min_width;
      create_options.min_height = min_height;
      create_options.max_width = max_width;
      create_options.max_height = max_height;
      create_options.modal = modal;
      create_options.display_id = display_id;
      LynxtronCreateHarmonyWindow(window_id_, &create_options);
    }

    InitFromOptions(options);
    std::lock_guard<std::mutex> lock(g_harmony_window_mutex);
    g_harmony_window = weak_factory_.GetWeakPtr();
  }

  ~NativeWindowHarmony() override {
    OH_LOG_INFO(LOG_APP,
                "[Window] NativeWindowHarmony destroyed window_id=%{public}d "
                "harmony_id=%{public}d",
                window_id_, harmony_window_id_);
    {
      std::lock_guard<std::mutex> window_lock(g_harmony_window_mutex);
      if (g_harmony_window.get() == this) {
        g_harmony_window.reset();
      }
      std::lock_guard<std::mutex> window_map_lock(g_window_map_mutex);
      g_id_to_window.erase(window_id_);
      if (harmony_window_id_ > 0) {
        g_harmony_id_to_window.erase(harmony_window_id_);
      }
    }
    // Drop the ArkTS callback as well; the Ability may already be gone.
    {
      std::lock_guard<std::mutex> lock(g_window_op_mutex);
      if (harmony_window_id_ > 0) {
        g_window_op_callbacks.erase(harmony_window_id_);
      }
    }
    // Drop the per-window windowless renderer/surface state so a late event
    // routed to the old harmony id cannot reach a stale renderer (which would
    // otherwise crash when its copied input callbacks dangle after the view is
    // destroyed).
    ReleaseHarmonyWindowRenderer(window_id_, harmony_window_id_);
  }

  void OnSurfaceSizeChanged(int width, int height) {
    if (width <= 0 || height <= 0) {
      return;
    }
    const bool size_changed =
        bounds_.width() != width || bounds_.height() != height;
    if (size_changed) {
      OH_LOG_INFO(LOG_APP,
                  "[Window] XComponent size %{public}dx%{public}d -> "
                  "%{public}dx%{public}d",
                  bounds_.width(), bounds_.height(), width, height);
      bounds_.set_size(gfx::Size(width, height));
    }
    // Notify on size changes AND on the first surface arrival. The first
    // arrival can happen after the LynxView was already built (delayed bind),
    // in which case the view's initial frame was dropped by the placeholder
    // renderer and must be re-laid-out/re-rendered now.
    if (size_changed || !surface_bound_) {
      surface_bound_ = true;
      NotifyWindowResize();
      NotifyWindowResized();
    }
  }

  // --- lifecycle ---
  void Close() override {
    OH_LOG_INFO(LOG_APP, "[Window] Close");
    NotifyWindowCloseButtonClicked();
  }
  void CloseImmediately() override {
    OH_LOG_INFO(LOG_APP, "[Window] CloseImmediately");
    InvokeWindowOp(harmony_window_id_, "close");
    NotifyWindowClosed();
    WindowList::RemoveWindow(this);
  }

  // --- focus / visibility ---
  void Focus(bool focus) override {
    is_focused_ = focus;
    OH_LOG_INFO(LOG_APP, "[Window] Focus(%{public}d)", static_cast<int>(focus));
    InvokeWindowOp(harmony_window_id_, "focus", focus ? "true" : "false");
    if (focus) {
      NotifyWindowFocus();
    } else {
      NotifyWindowBlur();
    }
  }
  bool IsFocused() override { return is_focused_; }
  void Show() override {
    is_visible_ = true;
    OH_LOG_INFO(LOG_APP, "[Window] Show");
    // Harmony window show is handled entirely on the ArkTS side; do not call
    // the native window_manager C API here to avoid a duplicate show path and
    // a hard dependency on libnative_window_manager.
    InvokeWindowOp(harmony_window_id_, "show");
    NotifyWindowShow();
  }
  void ShowInactive() override { is_visible_ = true; }
  void Hide() override {
    is_visible_ = false;
    OH_LOG_INFO(LOG_APP, "[Window] Hide");
    InvokeWindowOp(harmony_window_id_, "hide");
    NotifyWindowHide();
  }
  bool IsVisible() override { return is_visible_; }
  bool IsEnabled() override { return is_enabled_; }
  void SetEnabled(bool enable) override { is_enabled_ = enable; }

  // --- window state ---
  void Maximize() override {
    is_maximized_ = true;
    OH_LOG_INFO(LOG_APP, "[Window] Maximize");
    InvokeWindowOp(harmony_window_id_, "maximize");
    NotifyWindowMaximize();
  }
  void Unmaximize() override {
    is_maximized_ = false;
    OH_LOG_INFO(LOG_APP, "[Window] Unmaximize");
    NotifyWindowUnmaximize();
  }
  bool IsMaximized() const override { return is_maximized_; }
  void Minimize() override {
    is_minimized_ = true;
    OH_LOG_INFO(LOG_APP, "[Window] Minimize");
    InvokeWindowOp(harmony_window_id_, "minimize");
    NotifyWindowMinimize();
  }
  void Restore() override {
    is_minimized_ = false;
    is_maximized_ = false;
    OH_LOG_INFO(LOG_APP, "[Window] Restore");
    InvokeWindowOp(harmony_window_id_, "restore");
    NotifyWindowRestore();
  }
  bool IsMinimized() const override {
    if (IsClosed()) {
      return false;
    }
    return is_minimized_;
  }
  void SetFullScreen(bool fullscreen) override {
    if (is_fullscreen_ == fullscreen) {
      return;
    }
    is_fullscreen_ = fullscreen;
    OH_LOG_INFO(LOG_APP, "[Window] SetFullScreen(%{public}d)", static_cast<int>(fullscreen));
    if (fullscreen) {
      InvokeWindowOp(harmony_window_id_, "enter-full-screen");
      NotifyWindowEnterFullScreen();
    } else {
      InvokeWindowOp(harmony_window_id_, "leave-full-screen");
      NotifyWindowLeaveFullScreen();
    }
  }
  bool IsFullscreen() const override { return is_fullscreen_; }

  // --- geometry ---
  void SetBounds(const gfx::Rect& bounds, bool animate) override {}
  void SetPosition(const gfx::Point& position, bool animate) override {}
  void SetSize(const gfx::Size& size, bool animate) override {}
  gfx::Rect GetBounds() const override { return bounds_; }
  float GetDevicePixelRatio() const override { return device_pixel_ratio_; }
  gfx::Rect GetNormalBounds() const override { return bounds_; }
  gfx::Rect GetWindowBounds() const override { return window_bounds_; }
  gfx::Size GetWindowSize() const override { return window_bounds_.size(); }

  // Updates the cached window rect (windowRect) reported by ArkTS. This is the
  // actual OS window size including decorations/avoidance areas, which differs
  // from bounds_ (the drawable/content rect).
  void SetWindowBoundsFromArkTS(const gfx::Rect& rect) { window_bounds_ = rect; }

  // --- size constraints ---
  void SetSizeConstraints(const SizeConstraints& window_constraints) override {
    NativeWindow::SetSizeConstraints(window_constraints);
    InvokeWindowOp(harmony_window_id_, "setWindowLimits",
                   SizeConstraintsToJson(window_constraints).c_str());
  }
  void SetContentSizeConstraints(const SizeConstraints& size_constraints) override {
    NativeWindow::SetContentSizeConstraints(size_constraints);
    InvokeWindowOp(harmony_window_id_, "setWindowLimits",
                   SizeConstraintsToJson(size_constraints).c_str());
  }

  // --- resizable / movable ---
  void SetResizable(bool resizable) override { is_resizable_ = resizable; }
  void MoveTop() override {}
  bool IsResizable() const override { return is_resizable_; }
  void SetMovable(bool movable) override { is_movable_ = movable; }
  bool IsMovable() const override { return is_movable_; }
  void SetMinimizable(bool m) override { is_minimizable_ = m; }
  bool IsMinimizable() const override { return is_minimizable_; }
  void SetMaximizable(bool m) override { is_maximizable_ = m; }
  bool IsMaximizable() const override { return is_maximizable_; }
  void SetFullScreenable(bool f) override { is_fullscreenable_ = f; }
  bool IsFullScreenable() const override { return is_fullscreenable_; }
  void SetClosable(bool c) override { is_closable_ = c; }
  bool IsClosable() const override { return is_closable_; }

  // --- z-order / title ---
  void SetAlwaysOnTop(ui::ZOrderLevel z_order,
                      const std::string& level,
                      int relativeLevel) override {
    bool old_on_top = (z_order_ != ui::ZOrderLevel::kNormal);
    z_order_ = z_order;
    bool on_top = (z_order != ui::ZOrderLevel::kNormal);
    OH_LOG_INFO(LOG_APP, "[Window] SetAlwaysOnTop %{public}d", static_cast<int>(on_top));
    InvokeWindowOp(harmony_window_id_, "setAlwaysOnTop", on_top ? "true" : "false");
    if (old_on_top != on_top) {
      NotifyWindowAlwaysOnTopChanged();
    }
  }
  ui::ZOrderLevel GetZOrderLevel() const override { return z_order_; }
  // Center() is not part of the HarmonyOS window adaptation scope.
  void Center() override {}
  void SetTitle(const std::string& title) override {
    title_ = title;
    g_harmony_window_title = title;
  }
  std::string GetTitle() const override { return title_; }
  // GetAlwaysOnTopLevel / SetActive / IsActive are MAC-only.

  // --- misc window chrome (no-op on harmony) ---
  void FlashFrame(bool flash) override {}
  void SetSkipTaskbar(bool skip) override {}
  void SetExcludedFromShownWindowsMenu(bool excluded) override {}
  bool IsExcludedFromShownWindowsMenu() override { return false; }
  void SetSimpleFullScreen(bool simple_fullscreen) override {
    is_fullscreen_ = simple_fullscreen;
  }
  bool IsSimpleFullScreen() override { return is_fullscreen_; }
  void SetHasShadow(bool has_shadow) override { has_shadow_ = has_shadow; }
  bool HasShadow() override { return has_shadow_; }
  void SetOpacity(const double opacity) override { opacity_ = opacity; }
  double GetOpacity() override { return opacity_; }
  void SetFocusable(bool focusable) override { is_focusable_ = focusable; }
  bool IsFocusable() const override { return is_focusable_; }
  void SetParentWindow(NativeWindow* parent) override {
    // Keep NativeWindow::parent() in sync with BaseWindow's parent/child graph.
    // The old no-op left the native layer pointing at the constructor-time
    // parent even after JS called setParentWindow().
    NativeWindow::SetParentWindow(parent);

    if (!parent) {
      needs_harmony_parent_apply_ = false;
      // HarmonyOS only exposes Window.setParentWindow(windowId), and documents
      // it for changing one subwindow's parent to another.  There is no API for
      // converting a live subwindow into a top-level Ability window.  The
      // BaseWindow graph is still detached here, which preserves the public JS
      // getParentWindow()/getChildWindows() contract.
      OH_LOG_INFO(LOG_APP,
                  "[LynxtronWindow] detached logical parent window_id=%{public}d; "
                  "HarmonyOS has no live subwindow-to-top-level conversion API",
                  window_id_);
      return;
    }

    // A constructor-time `parent` is already applied by ArkTS through
    // WindowStage.createSubWindowWithOptions().  Calling
    // Window.setParentWindow() again for that initial relation is not merely
    // redundant: API 20 may reject it as an invalid parent.  Mark only a real
    // post-construction setParentWindow() call for cross-layer application.
    needs_harmony_parent_apply_ = true;
    ApplyParentWindowToHarmony();
  }

  // Re-run after ArkTS has bound both Harmony window ids and registered the
  // child's operation callback.  JS can call setParentWindow() immediately
  // after construction, before asynchronous Harmony window creation finishes.
  void ApplyParentWindowToHarmony() {
    if (!needs_harmony_parent_apply_) {
      return;
    }
    auto* parent_harmony = static_cast<NativeWindowHarmony*>(parent());
    if (!parent_harmony) {
      return;
    }
    const int32_t parent_harmony_id = parent_harmony->harmony_window_id();
    if (harmony_window_id_ <= 0 || parent_harmony_id <= 0) {
      OH_LOG_WARN(LOG_APP,
                  "[LynxtronWindow] defer native setParentWindow child=%{public}d "
                  "childHarmony=%{public}d parent=%{public}d parentHarmony=%{public}d",
                  window_id_, harmony_window_id_, parent_harmony->window_id(),
                  parent_harmony_id);
      return;
    }

    InvokeWindowOp(harmony_window_id_, "setParentWindow",
                   std::to_string(parent_harmony_id).c_str());
  }

  // --- window button (decor + three-button) visibility ---
  void SetWindowButtonVisibility(bool visible) override {
    is_window_buttons_visible_ = visible;
    DispatchWindowCommand(visible ? kWinCmdShowDecor : kWinCmdHideDecor);
  }
  bool GetWindowButtonVisibility() const override {
    return is_window_buttons_visible_;
  }

  // --- native handles (GetNativeWindow is MAC-only) ---
  NativeWindowHandle GetNativeWindowHandle() const override {
    return surface_;
  }

  // --- id accessors for cross-layer binding ---
  int32_t window_id() const {
    return window_id_;
  }
  int32_t harmony_window_id() const {
    return harmony_window_id_;
  }
  void SetHarmonyWindowId(int32_t id) {
    harmony_window_id_ = id;
  }
  int32_t GetHarmonyWindowId() const override {
    return harmony_window_id_;
  }
  int32_t GetCppWindowId() const override {
    return window_id_;
  }
  base::WeakPtr<NativeWindowHarmony> GetHarmonyWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // --- progress / workspaces ---
  void SetProgressBar(double progress, const ProgressState state) override {}
  void SetVisibleOnAllWorkspaces(bool visible,
                                 bool visibleOnFullScreen,
                                 bool skipTransformProcessType) override {}
  bool IsVisibleOnAllWorkspaces() override { return false; }

  // Traffic Light position / tabbing APIs remain MAC-only (guarded by
  // BUILDFLAG(IS_MAC) in native_window.h). Window button visibility is
  // shared with HarmonyOS and implemented above.

 protected:
  // No window chrome on harmony yet — content bounds == window bounds.
  gfx::Rect ContentBoundsToWindowBounds(
      const gfx::Rect& bounds) const override {
    return bounds;
  }
  gfx::Rect WindowBoundsToContentBounds(
      const gfx::Rect& bounds) const override {
    return bounds;
  }

 private:
  static std::string SizeConstraintsToJson(const SizeConstraints& constraints) {
    gfx::Size min_size = constraints.GetMinimumSize();
    gfx::Size max_size = constraints.GetMaximumSize();
    return "{\"minWidth\":" + std::to_string(min_size.width()) +
           ",\"minHeight\":" + std::to_string(min_size.height()) +
           ",\"maxWidth\":" + std::to_string(max_size.width()) +
           ",\"maxHeight\":" + std::to_string(max_size.height()) + "}";
  }

  gfx::Rect bounds_;             // Surface/content bounds (drawable rect).
  gfx::Rect window_bounds_;      // Window rect including system decorations.
  std::string title_;
  void* surface_ = nullptr;  // OHOS XComponent surface, set by HAP later.

  // C++-allocated id used for internal lookup and future JS-driven creation.
  int32_t window_id_ = -1;
  // HarmonyOS system window id published by ArkTS via setWindowId(). Used as
  // the cross-layer routing key in the current auto-started ability model.
  int32_t harmony_window_id_ = -1;

  bool is_focused_ = false;
  bool is_visible_ = false;
  bool is_enabled_ = true;
  bool is_maximized_ = false;
  bool is_minimized_ = false;
  bool is_fullscreen_ = false;
  bool is_resizable_ = true;
  bool is_movable_ = true;
  bool is_minimizable_ = true;
  bool is_maximizable_ = true;
  bool is_fullscreenable_ = true;
  bool is_closable_ = true;
  bool is_active_ = false;
  // True once the XComponent surface has arrived for this window, so the first
  // arrival triggers a viewport refresh even when the surface size happens to
  // equal the requested window size.
  bool surface_bound_ = false;
  bool is_focusable_ = true;
  // Initial parented windows are created as ArkUI subwindows directly.  This
  // flag is set only for a later JS setParentWindow(parent) invocation.
  bool needs_harmony_parent_apply_ = false;
  bool has_shadow_ = true;
  bool is_window_buttons_visible_ = true;
  double opacity_ = 1.0;
  float device_pixel_ratio_ = 1.0f;
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
  base::WeakPtrFactory<NativeWindowHarmony> weak_factory_{this};
};

// Called from lynxtron_napi_bridge.cc via dlsym. visibility("default") keeps
// it in the dynamic symbol table despite -fvisibility=hidden + stripping.
extern "C" __attribute__((visibility("default"))) void LynxtronSetWindowId(int32_t id) {
  OH_LOG_INFO(LOG_APP, "[LynxtronWindow] legacy SetWindowId id=%{public}d", id);

  // Legacy path used only by the first EntryAbility window, which has no
  // C++ window id. Bind the HarmonyOS id to the single native window that has
  // not received an id yet. If multiple windows are unbound the target is
  // ambiguous, so ignore and let LynxtronOnHarmonyWindowCreated handle them.
  std::lock_guard<std::mutex> lock(g_window_map_mutex);
  if (id <= 0) {
    return;
  }

  NativeWindowHarmony* unbound_window = nullptr;
  for (const auto& pair : g_id_to_window) {
    NativeWindowHarmony* w = pair.second;
    if (w && w->harmony_window_id() <= 0) {
      if (unbound_window) {
        OH_LOG_WARN(LOG_APP,
                    "[LynxtronWindow] SetWindowId: multiple unbound windows, "
                    "cannot bind ambiguous id=%{public}d",
                    id);
        return;
      }
      unbound_window = w;
    }
  }

  if (!unbound_window) {
    // The first EntryAbility window calls setWindowId before NativeWindowHarmony
    // is constructed. Cache the id so the first NativeWindowHarmony can consume
    // it when it is created.
    if (!g_pending_legacy_window_id.has_value()) {
      g_pending_legacy_window_id = id;
      OH_LOG_INFO(LOG_APP,
                  "[LynxtronWindow] SetWindowId: no unbound window yet, "
                  "caching id=%{public}d for first NativeWindowHarmony",
                  id);
    } else {
      OH_LOG_WARN(LOG_APP,
                  "[LynxtronWindow] SetWindowId: no unbound window and pending "
                  "id already exists, id=%{public}d ignored", id);
    }
    return;
  }

  unbound_window->SetHarmonyWindowId(id);
  g_harmony_id_to_window[id] = unbound_window;
  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] bound harmony id=%{public}d to "
              "window_id=%{public}d",
              id, unbound_window->window_id());
}

// Called from lynxtron_napi_bridge.cc via dlsym when the ArkTS side has
// finished creating a window requested by LynxtronCreateHarmonyWindow.
extern "C" __attribute__((visibility("default"))) void LynxtronOnHarmonyWindowCreated(
    int32_t cpp_window_id,
    int32_t harmony_window_id) {
  if (cpp_window_id <= 0 || harmony_window_id <= 0) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] OnHarmonyWindowCreated invalid ids "
                "cpp=%{public}d harmony=%{public}d",
                cpp_window_id, harmony_window_id);
    return;
  }
  NativeWindowHarmony* window = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_window_map_mutex);
    auto it = g_id_to_window.find(cpp_window_id);
    if (it != g_id_to_window.end()) window = it->second;
  }
  if (!window) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] OnHarmonyWindowCreated no window for "
                "cpp_id=%{public}d",
                cpp_window_id);
    return;
  }
  if (window->harmony_window_id() > 0) {
    g_harmony_id_to_window.erase(window->harmony_window_id());
  }
  window->SetHarmonyWindowId(harmony_window_id);
  g_harmony_id_to_window[harmony_window_id] = window;
  // If the LynxView was built before the HarmonyOS id was bound, its renderer
  // is a cpp-keyed placeholder. Re-key it now so the surface callback binds the
  // real renderer instead of letting the view keep another window's renderer.
  BindHarmonyWindowIdForPlaceholder(cpp_window_id, harmony_window_id);
  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] bound harmony id=%{public}d to cpp id=%{public}d",
              harmony_window_id, cpp_window_id);
}

// Called from surface_render_harmony.cc for backward compatibility.
// Deprecated: use LynxtronSetHarmonySurfaceSizeForWindow with an explicit id.
extern "C" __attribute__((visibility("default"))) void LynxtronSetHarmonySurfaceSize(
    int width,
    int height) {
  LynxtronSetHarmonySurfaceSizeForWindow(-1, width, height);
}

// Per-window surface-size update: routes the new XComponent size to the native
// window bound to the given harmony id (single-window fallback when unknown).
extern "C" __attribute__((visibility("default"))) void LynxtronSetHarmonySurfaceSizeForWindow(
    int32_t harmony_window_id,
    int width,
    int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  if (!GlobalThread::IsThreadInitialized(GlobalThread::UI)) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] SetHarmonySurfaceSizeForWindow: UI thread not "
                "ready, dropping %{public}dx%{public}d",
                width, height);
    return;
  }

  NativeWindowHarmony* target = nullptr;
  size_t window_count = 0;
  {
    std::lock_guard<std::mutex> lock(g_window_map_mutex);
    window_count = g_id_to_window.size();
    if (harmony_window_id > 0) {
      auto it = g_harmony_id_to_window.find(harmony_window_id);
      if (it != g_harmony_id_to_window.end()) {
        target = it->second;
      }
    }
    if (!target && window_count == 1) {
      target = g_id_to_window.begin()->second;
    }
  }

  if (!target) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] SetHarmonySurfaceSizeForWindow: no target for "
                "harmony_id=%{public}d (%{public}zu windows), dropping "
                "%{public}dx%{public}d",
                harmony_window_id, window_count, width, height);
    return;
  }

  // Update the native window bounds directly from the surface size. Keep this
  // path independent of the SetBounds() property setter so surface-driven sizing
  // does not re-enter the window-op dispatch path.
  base::WeakPtr<NativeWindowHarmony> weak_target = target->GetHarmonyWeakPtr();
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<NativeWindowHarmony> window, int w, int h) {
            if (!window) {
              return;
            }
            window->OnSurfaceSizeChanged(w, h);
            OH_LOG_INFO(LOG_APP,
                        "[LynxtronWindow] updated window_id=%{public}d to surface "
                        "%{public}dx%{public}d",
                        window->window_id(), w, h);
          },
          std::move(weak_target), width, height));
}

// Called from lynxtron_napi_bridge.cc via dlsym. Register the ArkTS callback
// for a specific window id so C++ can route minimize/restore/etc. to the right
// Ability instance. visibility("default") keeps it dlsym-able (see above).
extern "C" __attribute__((visibility("default"))) void LynxtronRegisterWindowOpCallbackForWindow(
    int32_t window_id,
    WindowOpCallback callback) {
  {
    std::lock_guard<std::mutex> lock(g_window_op_mutex);
    if (callback) {
      g_window_op_callbacks[window_id] = callback;
    } else {
      g_window_op_callbacks.erase(window_id);
    }
  }
  OH_LOG_INFO(LOG_APP, "[LynxtronWindow] registered callback id=%{public}d", window_id);

  if (!callback) {
    return;
  }

  // Apply a relationship that was requested while either window was still
  // being created.  Also retry children waiting for this newly-bound parent.
  std::vector<base::WeakPtr<NativeWindowHarmony>> relationships_to_apply;
  {
    std::lock_guard<std::mutex> lock(g_window_map_mutex);
    auto changed_it = g_harmony_id_to_window.find(window_id);
    if (changed_it != g_harmony_id_to_window.end()) {
      NativeWindowHarmony* changed = changed_it->second;
      relationships_to_apply.push_back(changed->GetHarmonyWeakPtr());
      for (const auto& entry : g_id_to_window) {
        NativeWindowHarmony* candidate = entry.second;
        if (candidate && candidate->parent() == changed) {
          relationships_to_apply.push_back(candidate->GetHarmonyWeakPtr());
        }
      }
    }
  }
  for (const auto& relationship : relationships_to_apply) {
    if (relationship) {
      relationship->ApplyParentWindowToHarmony();
    }
  }
}

// Returns the C++-allocated id of the only native window in this process.
// In multi-window mode the caller must know which window it is asking about;
// this legacy helper therefore returns -1 when more than one window exists.
extern "C" __attribute__((visibility("default"))) int32_t LynxtronGetWindowId() {
  std::lock_guard<std::mutex> lock(g_window_map_mutex);
  if (g_id_to_window.size() != 1) {
    return -1;
  }
  return g_id_to_window.begin()->first;
}

// ArkTS reports window state changes (foreground/background/etc.) using the
// HarmonyOS window id it already knows. Map it back to the native instance and
// update observers.
namespace {

// Converts the plain-C resize edge passed across the dlopen boundary into the
// C++ gfx::ResizeEdge used by the observer pipeline.
gfx::ResizeEdge ToResizeEdge(int32_t edge) {
  switch (edge) {
    case kLynxtronResizeEdgeBottom:
      return gfx::ResizeEdge::kBottom;
    case kLynxtronResizeEdgeBottomLeft:
      return gfx::ResizeEdge::kBottomLeft;
    case kLynxtronResizeEdgeBottomRight:
      return gfx::ResizeEdge::kBottomRight;
    case kLynxtronResizeEdgeLeft:
      return gfx::ResizeEdge::kLeft;
    case kLynxtronResizeEdgeRight:
      return gfx::ResizeEdge::kRight;
    case kLynxtronResizeEdgeTop:
      return gfx::ResizeEdge::kTop;
    case kLynxtronResizeEdgeTopLeft:
      return gfx::ResizeEdge::kTopLeft;
    case kLynxtronResizeEdgeTopRight:
      return gfx::ResizeEdge::kTopRight;
    default:
      return gfx::ResizeEdge::kBottomRight;
  }
}

// Maps an ArkTS-reported window state string ("foreground", "resize", ...) to
// the matching window observer notification for the bound native window.
void DispatchHarmonyWindowState(
    int32_t harmony_window_id,
    const std::string& state,
    int32_t resize_edge) {
  NativeWindowHarmony* window = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_window_map_mutex);
    auto it = g_harmony_id_to_window.find(harmony_window_id);
    if (it != g_harmony_id_to_window.end()) window = it->second;
  }
  if (!window) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] dispatch: no native window for "
                "harmony_id=%{public}d state=%{public}s (was setWindowId / "
                "setWindowIdForWindow called?)",
                harmony_window_id, state.c_str());
    return;
  }

  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] dispatch harmony_id=%{public}d "
              "window_id=%{public}d state=%{public}s",
              harmony_window_id, window->window_id(), state.c_str());

  if (state == "foreground") {
    window->NotifyWindowFocus();
  } else if (state == "background") {
    window->NotifyWindowBlur();
  } else if (state == "minimize") {
    window->NotifyWindowMinimize();
  } else if (state == "restore") {
    window->NotifyWindowRestore();
  } else if (state == "maximize") {
    window->NotifyWindowMaximize();
  } else if (state == "enter-full-screen") {
    window->NotifyWindowEnterFullScreen();
  } else if (state == "leave-full-screen") {
    window->NotifyWindowLeaveFullScreen();
  } else if (state == "show") {
    window->NotifyWindowShow();
  } else if (state == "hide") {
    window->NotifyWindowHide();
  } else if (state == "blur") {
    window->NotifyWindowBlur();
  } else if (state == "close") {
    // Ask observers (JS `close` handler) before actually closing.
    window->NotifyWindowCloseButtonClicked();
  } else if (state == "closed") {
    window->NotifyWindowClosed();
  } else if (state == "resize") {
    window->NotifyWindowResize();
  } else if (state == "resized") {
    window->NotifyWindowResized();
  } else if (state == "move") {
    window->NotifyWindowMove();
  } else if (state == "moved") {
    window->NotifyWindowMoved();
  } else if (state == "will-resize") {
    const gfx::ResizeEdge edge =
        ToResizeEdge(resize_edge);
    bool prevent_default = false;
    OH_LOG_INFO(LOG_APP, "[LynxtronWindow] will-resize edge=%{public}d",
                static_cast<int>(edge));
    // HarmonyOS has no prospective size at DRAG_START (the rect has not
    // changed yet), so pass the current bounds; the meaningful payload is the
    // edge computed by ArkTS from the mouse position and window rect.
    window->NotifyWindowWillResize(window->GetBounds(), edge, prevent_default);
    OH_LOG_INFO(LOG_APP,
                "[LynxtronWindow] will-resize prevent_default=%{public}d",
                prevent_default);
  } else {
    OH_LOG_WARN(LOG_APP, "[LynxtronWindow] unknown state=%{public}s",
                state.c_str());
  }
}

// Updates the cached OS window rect (decorations included) reported by ArkTS
// for the native window bound to the given harmony id.
void DispatchHarmonyWindowRect(int32_t harmony_window_id,
                               const gfx::Rect& rect) {
  NativeWindowHarmony* window = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_window_map_mutex);
    auto it = g_harmony_id_to_window.find(harmony_window_id);
    if (it != g_harmony_id_to_window.end()) {
      window = it->second;
    }
  }
  if (!window) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] dispatch rect: no native window for "
                "harmony_id=%{public}d",
                harmony_window_id);
    return;
  }

  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] dispatch rect harmony_id=%{public}d "
              "rect=%{public}d,%{public}d,%{public}dx%{public}d",
              harmony_window_id, rect.x(), rect.y(), rect.width(),
              rect.height());
  window->SetWindowBoundsFromArkTS(rect);
}

}  // namespace

// Per-window XComponent size update: posts OnSurfaceSizeChanged for the bound
// native window on the UI thread (single-window fallback when the id is unknown).
void UpdateHarmonyNativeWindowSizeForWindow(int32_t harmony_window_id,
                                            int width,
                                            int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  NativeWindowHarmony* target = nullptr;
  size_t window_count = 0;
  {
    std::lock_guard<std::mutex> lock(g_window_map_mutex);
    window_count = g_id_to_window.size();
    if (harmony_window_id > 0) {
      auto it = g_harmony_id_to_window.find(harmony_window_id);
      if (it != g_harmony_id_to_window.end()) {
        target = it->second;
      }
    }
    if (!target && window_count == 1) {
      target = g_id_to_window.begin()->second;
    }
  }
  if (!target) {
    OH_LOG_WARN(LOG_APP,
                "[Window] UpdateHarmonyNativeWindowSizeForWindow: no target for "
                "harmony_id=%{public}d (%{public}zu windows), dropping "
                "%{public}dx%{public}d",
                harmony_window_id, window_count, width, height);
    return;
  }

  auto runner = GetUIThreadTaskRunner();
  if (!runner) {
    OH_LOG_ERROR(LOG_APP, "[Window] no UI runner for XComponent size update");
    return;
  }
  base::WeakPtr<NativeWindowHarmony> weak_window = target->GetHarmonyWeakPtr();
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<NativeWindowHarmony> window, int width, int height) {
            if (window) {
              window->OnSurfaceSizeChanged(width, height);
            }
          },
          std::move(weak_window), width, height));
}

// Legacy single-window XComponent size update routed to the most recent native
// window.
void UpdateHarmonyNativeWindowSize(int width, int height) {
  base::WeakPtr<NativeWindowHarmony> window;
  {
    std::lock_guard<std::mutex> lock(g_harmony_window_mutex);
    window = g_harmony_window;
  }
  if (!window || width <= 0 || height <= 0) {
    return;
  }

  auto runner = GetUIThreadTaskRunner();
  if (!runner) {
    OH_LOG_ERROR(LOG_APP, "[Window] no UI runner for XComponent size update");
    return;
  }
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<NativeWindowHarmony> window, int width, int height) {
            if (window) {
              window->OnSurfaceSizeChanged(width, height);
            }
          },
          std::move(window), width, height));
}

// Entry point for ArkTS window-state reports: validates the args and posts the
// state dispatch to the UI thread (see DispatchHarmonyWindowState).
extern "C" __attribute__((visibility("default"))) void LynxtronNotifyWindowState(
    int32_t harmony_window_id,
    const char* state,
    int32_t resize_edge) {
  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] notifyWindowState entry harmony_id=%{public}d "
              "state=%{public}s resizeEdge=%{public}d",
              harmony_window_id, state ? state : "(null)", resize_edge);

  if (!state || harmony_window_id <= 0) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] notifyWindowState: invalid args, dropping "
                "(harmony_id=%{public}d state=%{public}s)",
                harmony_window_id, state ? state : "(null)");
    return;
  }

  // ArkTS lifecycle callbacks run inside the ArkUI / NAPI runtime. The window
  // observers that eventually emit JS events must run on the C++ UI thread's
  // task runner where a V8 context is active. Even if the calling pthread is
  // the same as the C++ UI thread, we are not inside the base message loop that
  // V8 is tied to, so always PostTask instead of calling directly.
  if (!GlobalThread::IsThreadInitialized(GlobalThread::UI)) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] notifyWindowState: UI thread not ready, "
                "dropping harmony_id=%{public}d state=%{public}s",
                harmony_window_id, state);
    return;
  }

  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] posting state dispatch to UI thread "
              "(harmony_id=%{public}d)",
              harmony_window_id);

  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](int32_t id, const std::string& state, int32_t edge) {
            DispatchHarmonyWindowState(id, state, edge);
          },
          harmony_window_id, std::string(state), resize_edge));
}

// Entry point for ArkTS window-rect reports: validates the args and posts the
// rect dispatch to the UI thread (see DispatchHarmonyWindowRect).
extern "C" __attribute__((visibility("default"))) void LynxtronNotifyWindowRect(
    int32_t harmony_window_id,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {
  OH_LOG_INFO(LOG_APP,
              "[LynxtronWindow] notifyWindowRect entry harmony_id=%{public}d "
              "rect=%{public}d,%{public}d,%{public}dx%{public}d",
              harmony_window_id, x, y, width, height);

  if (harmony_window_id <= 0 || width <= 0 || height <= 0) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] notifyWindowRect: invalid args, dropping "
                "(harmony_id=%{public}d rect=%{public}d,%{public}d,%{public}dx%{public}d)",
                harmony_window_id, x, y, width, height);
    return;
  }

  if (!GlobalThread::IsThreadInitialized(GlobalThread::UI)) {
    OH_LOG_WARN(LOG_APP,
                "[LynxtronWindow] notifyWindowRect: UI thread not ready, "
                "dropping harmony_id=%{public}d",
                harmony_window_id);
    return;
  }

  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](int32_t id, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
            DispatchHarmonyWindowRect(id, gfx::Rect(rx, ry, rw, rh));
          },
          harmony_window_id, x, y, width, height));
}

// NativeWindow factory: instantiates the HarmonyOS concrete implementation.
// static
NativeWindow* NativeWindow::Create(const gin_helper::Dictionary& options,
                                   NativeWindow* parent) {
  return new NativeWindowHarmony(options, parent);
}

}  // namespace lynxtron

// Returns the title of the most recently titled native window; consumed by the
// NAPI bridge to sync the ability mission label.
extern "C" __attribute__((visibility("default")))
const char* LynxtronGetWindowTitle() {
  return lynxtron::g_harmony_window_title.c_str();
}

// Registers the bridge's command handler. Called once by the NAPI bridge after
// the OHOS window object is available; NativeWindowHarmony then dispatches
// window-decor visibility changes straight to it (no polling).
extern "C" __attribute__((visibility("default")))
void LynxtronSetWindowCommandHandler(lynxtron::WindowCommandHandler handler) {
  lynxtron::g_window_command_handler = handler;
}
