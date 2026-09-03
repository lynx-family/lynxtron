// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/app/lynx_windowless_renderer_harmony.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <hilog/log.h>
#include <pthread.h>

#include <chrono>
#include <map>
#include <mutex>
#include <array>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "lynx/platform/embedder/windowless/lynx_windowless_renderer_priv.h"
#include "shell/common/global_thread.h"
#include "shell/app/application.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "LynxtronWLR"
#define WLR_LOG(fmt, ...) OH_LOG_INFO(LOG_APP, "[WLR] " fmt, ##__VA_ARGS__)
#define WLR_ERR(fmt, ...) OH_LOG_ERROR(LOG_APP, "[WLR] " fmt, ##__VA_ARGS__)

namespace lynxtron {

namespace {

std::mutex g_mutex;
scoped_refptr<base::SingleThreadTaskRunner> g_lynx_platform_runner;
HarmonyTextInputFocusCallback g_text_input_focus_callback = nullptr;

// The runner Clay treats as its "platform thread": the Lynxtron UI thread that
// built the LynxView (captured in LynxViewBuilder::Build). Clay's custom
// platform task runner funnels through OnPostTask below, so without this the
// runner is a black hole.
scoped_refptr<base::SingleThreadTaskRunner> LynxPlatformRunner() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_lynx_platform_runner;
}

// GLDirect windowless renderer whose GL callbacks drive an EGL context bound to
// the XComponent OHNativeWindow. Clay makes the context current on its own GPU
// thread (via OnGLMakeCurrent), renders into FBO 0 (the window's default
// framebuffer), and presents via eglSwapBuffers.
//
// EGL objects (display/surface/context) are created LAZILY on the first
// OnGLMakeCurrent, i.e. on Clay's GPU thread — NOT on the surface-callback
// thread that constructs this renderer. Creating the window surface + context
// on one thread and eglMakeCurrent-ing them on another triggers EGL_BAD_ACCESS
// (0x3002) on the OHOS EGL, which was leaving the frame only partly composited.
// Deferring keeps all EGL for this surface on the single GPU thread.
class EglWindowlessRenderer : public lynx::pub::LynxWindowlessRenderer {
 public:
  explicit EglWindowlessRenderer(void* window)
      : lynx::pub::LynxWindowlessRenderer(kRendererTypeGLDirect),
        window_(window) {}

  ~EglWindowlessRenderer() override {
    // The base dtor releases the capi renderer. Tear down EGL after that; the
    // GPU thread has stopped calling us by the time the view is destroyed.
    if (display_ != EGL_NO_DISPLAY) {
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
      if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
    }
  }

  // Creates the EGL display/config/surface/context on the CURRENT thread (Clay's
  // GPU thread). Returns false on any EGL failure.
  bool SetupEgl(void* window) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
      WLR_ERR("eglGetDisplay failed");
      return false;
    }
    EGLint major = 0, minor = 0;
    if (!eglInitialize(display_, &major, &minor)) {
      WLR_ERR("eglInitialize failed 0x%{public}x", eglGetError());
      return false;
    }
    WLR_LOG("EGL %{public}d.%{public}d", major, minor);

    const EGLint cfg_attrs[] = {EGL_SURFACE_TYPE,
                                EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                                EGL_RED_SIZE,        8,
                                EGL_GREEN_SIZE,      8,
                                EGL_BLUE_SIZE,       8,
                                EGL_ALPHA_SIZE,      8,
                                EGL_DEPTH_SIZE,      0,
                                EGL_STENCIL_SIZE,    8,
                                EGL_NONE};
    EGLint num_cfg = 0;
    if (!eglChooseConfig(display_, cfg_attrs, &cfg_, 1, &num_cfg) ||
        num_cfg < 1) {
      WLR_ERR("eglChooseConfig failed");
      return false;
    }

    surface_ = eglCreateWindowSurface(
        display_, cfg_, reinterpret_cast<EGLNativeWindowType>(window), nullptr);
    if (surface_ == EGL_NO_SURFACE) {
      WLR_ERR("eglCreateWindowSurface failed 0x%{public}x", eglGetError());
      return false;
    }

    const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, cfg_, EGL_NO_CONTEXT, ctx_attrs);
    if (context_ == EGL_NO_CONTEXT) {
      WLR_ERR("eglCreateContext failed 0x%{public}x", eglGetError());
      return false;
    }
    primary_thread_ = pthread_self();
    WLR_LOG("EGL surface+context ready for window=%{public}p (primary tid=%{public}lu)",
            window, (unsigned long)primary_thread_);
    return true;
  }

  // Returns the (surface, context) this thread should bind. The primary (first,
  // raster) thread gets the real window surface + context. Any other thread
  // (Clay's resource/IO thread) gets its OWN context that SHARES resources with
  // the primary, on a 1x1 pbuffer — the Flutter/Skia resource-context pattern.
  // This is what fixes EGL_BAD_ACCESS: the window surface stays owned by the
  // raster thread while resource threads upload textures on a shared context.
  std::pair<EGLSurface, EGLContext> SurfaceContextForThisThread() {
    pthread_t self = pthread_self();
    if (pthread_equal(self, primary_thread_)) {
      return {surface_, context_};
    }
    std::lock_guard<std::mutex> lock(threads_mutex_);
    auto it = per_thread_.find(self);
    if (it != per_thread_.end()) {
      return it->second;
    }
    const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext ctx = eglCreateContext(display_, cfg_, context_, ctx_attrs);
    const EGLint pb_attrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    EGLSurface pb = eglCreatePbufferSurface(display_, cfg_, pb_attrs);
    if (ctx == EGL_NO_CONTEXT || pb == EGL_NO_SURFACE) {
      WLR_ERR("resource ctx/pbuffer create failed 0x%{public}x", eglGetError());
    } else {
      WLR_LOG("resource ctx+pbuffer for tid=%{public}lu", (unsigned long)self);
    }
    auto pair = std::make_pair(pb, ctx);
    per_thread_[self] = pair;
    return pair;
  }

  void* window() const { return window_; }
  void SetHarmonyWindowId(int32_t id) { harmony_window_id_ = id; }
  int32_t GetHarmonyWindowId() const { return harmony_window_id_; }
  // Exposes the underlying C renderer so the placeholder can copy the input/
  // task callbacks that LynxUIRendererWindowless installed on it.
  lynx_windowless_renderer_t* capi_impl() { return Impl(); }

  // ---- GLDirect callbacks (invoked on Clay's GPU thread) ----

  bool OnGLMakeCurrent() override {
    // Lazily create EGL on the first call so all EGL for this surface lives on
    // this (Clay GPU) thread — avoids EGL_BAD_ACCESS from cross-thread binding.
    if (display_ == EGL_NO_DISPLAY) {
      if (!SetupEgl(window_)) {
        WLR_ERR("lazy SetupEgl failed on gpu thread");
        return false;
      }
    }
    auto [surf, ctx] = SurfaceContextForThisThread();
    if (eglMakeCurrent(display_, surf, surf, ctx)) {
      return true;
    }
    EGLint err = eglGetError();
    if (err == EGL_SUCCESS || eglGetCurrentContext() == ctx) {
      return true;
    }
    WLR_ERR("eglMakeCurrent failed 0x%{public}x (tid=%{public}lu)", err,
            (unsigned long)pthread_self());
    return false;
  }

  bool OnGLClearCurrent() override {
    return eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                          EGL_NO_CONTEXT);
  }

  // Present the window surface. Only meaningful on the primary/raster thread
  // whose bound surface IS the window surface; resource threads never present.
  bool OnGLPresent() override { return eglSwapBuffers(display_, surface_); }

  // FBO 0 == the EGL window surface's default framebuffer (renders on screen).
  uint32_t OnGLCreateFBO(int width, int height) override { return 0; }

  void* OnGLProcResolver(const char* name) override {
    return reinterpret_cast<void*>(eglGetProcAddress(name));
  }

  // Clay's custom platform task runner drains through here. Without an
  // override the base class silently drops every task, which strands anything
  // Clay schedules on its platform thread (IME commits, editor actions,
  // clipboard round-trips) forever. Run them on the Lynxtron UI thread — the
  // same thread that built the LynxView, which is what Clay was handed as its
  // platform runner.
  void OnPostTask(lynx_task_t task, uint64_t interval_nanoseconds) override {
    auto runner = LynxPlatformRunner();
    if (!runner) {
      WLR_ERR("OnPostTask dropped: no platform runner captured yet");
      return;
    }
    // The task holds a Clay-side handle that is only valid while this renderer
    // lives; drop it if the view went away before the task ran.
    auto closure = base::BindOnce(
        [](std::weak_ptr<lynx::pub::LynxWindowlessRenderer> weak,
           lynx_task_t t) {
          if (auto self = weak.lock()) {
            self->RunTask(t);
          }
        },
        weak_from_this(), task);
    if (interval_nanoseconds == 0) {
      runner->PostTask(FROM_HERE, std::move(closure));
    } else {
      runner->PostDelayedTask(FROM_HERE, std::move(closure),
                              base::Nanoseconds(interval_nanoseconds));
    }
  }

  // The platform text client is owned by ArkUI.  These callbacks can arrive on
  // a Lynx-owned sequence, so only cache POD state here; the NAPI bridge polls
  // it from the ArkUI sequence before moving/focusing its TextInput.
  // Notifies the NAPI bridge of the current text-input visibility/caret rect.
  // Must be called without holding input_mutex_ so the bridge can safely call
  // back into GetTextInputState() if needed.
  void NotifyTextInputFocus() {
    if (!g_text_input_focus_callback) {
      return;
    }
    float x = 0, y = 0, w = 1, h = 1;
    bool visible = false;
    {
      std::lock_guard<std::mutex> lock(input_mutex_);
      visible = text_input_visible_;
      x = marked_rect_[0] * editable_transform_[0] + editable_transform_[12];
      y = marked_rect_[1] * editable_transform_[5] + editable_transform_[13];
      w = marked_rect_[2];
      h = marked_rect_[3];
    }
    g_text_input_focus_callback(harmony_window_id_, visible, x, y, w, h);
  }

  // IME state setters: cache the latest value and notify the NAPI bridge so
  // the candidate window follows the caret.
  void ShowTextInput(bool show) override {
    {
      std::lock_guard<std::mutex> lock(input_mutex_);
      text_input_visible_ = show;
    }
    OH_LOG_INFO(LOG_APP, "[IME] ShowTextInput visible=%{public}d", show);
    NotifyTextInputFocus();
  }

  void SetMarkedTextRect(float x, float y, float width, float height) override {
    {
      std::lock_guard<std::mutex> lock(input_mutex_);
      marked_rect_ = {x, y, width, height};
    }
    OH_LOG_INFO(LOG_APP,
                "[IME] marked rect x=%{public}f y=%{public}f w=%{public}f h=%{public}f",
                x, y, width, height);
    NotifyTextInputFocus();
  }

  void UpdateCaretPosition(float x, float y, float width, float height) override {
    {
      std::lock_guard<std::mutex> lock(input_mutex_);
      marked_rect_ = {x, y, width, height};
    }
    OH_LOG_INFO(LOG_APP,
                "[IME] caret rect x=%{public}f y=%{public}f w=%{public}f h=%{public}f",
                x, y, width, height);
    NotifyTextInputFocus();
  }

  void SetEditableTransform(const float transform[16]) override {
    {
      std::lock_guard<std::mutex> lock(input_mutex_);
      for (size_t i = 0; i < editable_transform_.size(); ++i) {
        editable_transform_[i] = transform[i];
      }
    }
    OH_LOG_INFO(LOG_APP, "[IME] editable transform updated");
    NotifyTextInputFocus();
  }

  bool GetTextInputState(float* x, float* y, float* width, float* height) {
    std::lock_guard<std::mutex> lock(input_mutex_);
    // The Lynx callback uses view-local logical pixels. Apply the commonly
    // used scale/translate subset once, leaving ArkUI to add the XComponent's
    // own offset when it lays out the TextInput.
    if (x) *x = marked_rect_[0] * editable_transform_[0] + editable_transform_[12];
    if (y) *y = marked_rect_[1] * editable_transform_[5] + editable_transform_[13];
    if (width) *width = marked_rect_[2];
    if (height) *height = marked_rect_[3];
    return text_input_visible_;
  }

 private:
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLConfig cfg_ = nullptr;
  EGLSurface surface_ = EGL_NO_SURFACE;  // window surface (primary/raster thread)
  EGLContext context_ = EGL_NO_CONTEXT;  // primary context
  void* window_ = nullptr;
  int32_t harmony_window_id_ = -1;
  pthread_t primary_thread_ = 0;
  std::mutex threads_mutex_;
  // Per resource/IO thread: its own shared context + 1x1 pbuffer.
  std::map<pthread_t, std::pair<EGLSurface, EGLContext>> per_thread_;
  std::mutex input_mutex_;
  bool text_input_visible_ = false;
  std::array<float, 4> marked_rect_ = {0, 0, 1, 1};
  std::array<float, 16> editable_transform_ = {
      1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// Per-window renderer and surface state. Multi-window mode uses
// harmony_window_id as the routing key; single-window fallbacks read the most
// recently touched entry.
std::unordered_map<int32_t, std::shared_ptr<EglWindowlessRenderer>> g_renderers;
std::unordered_map<int32_t, std::pair<int, int>> g_surface_sizes;
int32_t g_last_harmony_window_id = -1;

// Placeholder renderers created by the LynxView build path when the XComponent
// surface has not arrived yet. They are keyed by harmony_window_id and are
// bound to the real renderer in CreateHarmonyWindowlessRenderer(). Stored as
// weak_ptr so the LynxView's ownership keeps them alive without us leaking refs.
std::unordered_map<int32_t, std::weak_ptr<lynx::pub::LynxWindowlessRenderer>>
    g_placeholder_renderers;

// Placeholder renderers keyed by the C++-allocated window id for the brief
// window between NativeWindowHarmony construction and the ArkTS side binding
// the HarmonyOS window id. BindHarmonyWindowIdForPlaceholder() moves them into
// g_placeholder_renderers once the HarmonyOS id is known.
std::unordered_map<int32_t, std::weak_ptr<lynx::pub::LynxWindowlessRenderer>>
    g_cpp_placeholder_renderers;

// C++ window id -> HarmonyOS window id mapping, recorded the moment the ArkTS
// side binds a window. The view build path can race that binding in either
// direction, so this map lets a placeholder created AFTER the binding be keyed
// by the HarmonyOS id (or resolve the already-created real renderer) directly.
std::unordered_map<int32_t, int32_t> g_cpp_to_harmony;

// Text-input focus/caret callback registered by the NAPI bridge. Invoked from
// EglWindowlessRenderer on the Lynx/Clay sequence; the bridge re-posts to the
// ArkUI thread as needed.

namespace {
std::shared_ptr<EglWindowlessRenderer> GetCurrentRendererLocked() {
  if (g_last_harmony_window_id > 0) {
    auto it = g_renderers.find(g_last_harmony_window_id);
    if (it != g_renderers.end()) return it->second;
  }
  if (!g_renderers.empty()) return g_renderers.begin()->second;
  return nullptr;
}
}  // namespace

// ---------------------------------------------------------------------------
// Global UI task runner for windowless mode
//
// Lynx's UIThread is what Clay/Lynx post their vsync ticks, animation frames
// and element patches onto. Left to itself, LynxUIRendererWindowless calls
// base::UIThread::Init(), which does
// fml::MessageLoop::EnsureInitializedForCurrentThread() on whatever thread
// built the view -- here the Lynxtron (chromium) UI thread. Chromium's RunLoop
// knows nothing about that fml loop and never pumps it, so every task posted to
// UIThread::GetRunner() is silently dropped forever: no vsync callback, no
// animation tick, no patch. The first frame still shows up because it goes
// through the synchronous load path.
//
// The windowless C API exists for exactly this: hand Lynx a delegate that
// forwards onto a runner the host actually drives. It must be installed before
// any windowless renderer is created (once installed, UIThread::InitTaskRunner
// is a no-op because HasInit() is already true).
//
// Note this is why the Windows build works without it: the UIThread::Init()
// call in LynxUIRendererWindowless is guarded by #if !defined(OS_WIN).
// ---------------------------------------------------------------------------

scoped_refptr<base::SingleThreadTaskRunner> LynxUiRunner() {
  auto runner = LynxPlatformRunner();
  if (!runner) {
    runner = lynxtron::GetUIThreadTaskRunner();
  }
  return runner;
}

bool UiRunsOnCurrentThread(void* /*user_data*/) {
  auto runner = LynxUiRunner();
  return runner && runner->RunsTasksInCurrentSequence();
}

// Clay hands us an absolute deadline on the same clock fml::TimePoint uses
// (steady_clock nanoseconds since epoch); convert it to a relative delay.
void UiPostTask(lynx_task_t task, uint64_t target_time_nanos,
                void* /*user_data*/) {
  auto runner = LynxUiRunner();
  if (!runner) {
    WLR_ERR("[UITASK] dropped: no UI runner yet");
    return;
  }
  auto closure = base::BindOnce([](lynx_task_t t) {
    lynx_windowless_run_ui_task(t);
  }, task);

  const uint64_t now = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
  if (target_time_nanos > now) {
    runner->PostDelayedTask(FROM_HERE, std::move(closure),
                            base::Nanoseconds(target_time_nanos - now));
  } else {
    runner->PostTask(FROM_HERE, std::move(closure));
  }
}

// Installs the delegate exactly once. Must run before the first LynxView (and
// therefore the first LynxUIRendererWindowless) is built.
void EnsureGlobalUiTaskRunner() {
  static std::once_flag once;
  std::call_once(once, [] {
    lynx_windowless_ui_task_runner_config_t config = {};
    config.struct_size = sizeof(config);
    config.user_data = nullptr;
    config.runs_on_current_thread_callback = &UiRunsOnCurrentThread;
    config.post_task_callback = &UiPostTask;
    const bool ok = lynx_windowless_set_global_ui_task_runner(&config);
    WLR_LOG("[UITASK] set_global_ui_task_runner ok=%{public}d", ok ? 1 : 0);
  });
}

}  // namespace

// Placeholder windowless renderer used when a LynxView is built before its
// XComponent surface has arrived. It creates its own C renderer object so the
// LynxView can be constructed immediately, and forwards all renderer/platform
// callbacks to the real renderer once CreateHarmonyWindowlessRenderer() binds
// it. This prevents multi-window setups from accidentally sharing another
// window's renderer during the gap.
class DelegatingWindowlessRenderer : public lynx::pub::LynxWindowlessRenderer {
 public:
  DelegatingWindowlessRenderer()
      : lynx::pub::LynxWindowlessRenderer(kRendererTypeGLDirect) {}

  ~DelegatingWindowlessRenderer() override {
    // SetTarget copied our input/task callbacks (which capture the
    // LynxUIRendererWindowless) into the real renderer's C impl. The
    // LynxUIRendererWindowless is destroyed before this placeholder, leaving
    // those copies dangling. Clear them so a late pointer/key event routed to
    // the real renderer by harmony id cannot call into freed memory (the
    // MAPERR-at-small-offset crash when closing a window).
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (!target_) {
      return;
    }
    auto* target_impl =
        reinterpret_cast<lynx::embedder::LynxWindowlessRenderer*>(
            target_->Impl());
    target_impl->send_pointer_event = nullptr;
    target_impl->send_key_event = nullptr;
    target_impl->run_task = nullptr;
  }

  void SetTarget(std::shared_ptr<lynx::pub::LynxWindowlessRenderer> target) {
    std::lock_guard<std::mutex> lock(target_mutex_);
    target_ = std::move(target);
    if (!target_) {
      return;
    }
    // Input (pointer/key) events and Clay tasks are routed by harmony window id
    // to the REAL renderer, while LynxUIRendererWindowless installed the
    // send_pointer_event/send_key_event/run_task callbacks on this placeholder
    // when the LynxView was built. Copy them over, otherwise those events are
    // silently dropped once the real renderer takes over.
    auto* self_impl = reinterpret_cast<lynx::embedder::LynxWindowlessRenderer*>(
        Impl());
    auto* target_egl = static_cast<EglWindowlessRenderer*>(target_.get());
    auto* target_impl = reinterpret_cast<lynx::embedder::LynxWindowlessRenderer*>(
        target_egl->capi_impl());
    if (self_impl->send_pointer_event) {
      target_impl->send_pointer_event = self_impl->send_pointer_event;
    }
    if (self_impl->send_key_event) {
      target_impl->send_key_event = self_impl->send_key_event;
    }
    if (self_impl->run_task) {
      target_impl->run_task = self_impl->run_task;
    }
  }

  bool HasTarget() const {
    std::lock_guard<std::mutex> lock(target_mutex_);
    return target_ != nullptr;
  }

  // ---- GLDirect callbacks ----
  bool OnGLMakeCurrent() override {
    const bool had_target = HasTarget();
    const bool result = Forward(&LynxWindowlessRenderer::OnGLMakeCurrent);
    if (had_target && !gl_fwd_logged_) {
      gl_fwd_logged_ = true;
      WLR_LOG("[WLR] placeholder OnGLMakeCurrent fwd result=%{public}d",
              result ? 1 : 0);
    }
    return result;
  }
  bool OnGLClearCurrent() override { return Forward(&LynxWindowlessRenderer::OnGLClearCurrent); }
  bool OnGLPresent() override {
    const bool had_target = HasTarget();
    const bool result = Forward(&LynxWindowlessRenderer::OnGLPresent);
    if (had_target && !gl_present_logged_) {
      gl_present_logged_ = true;
      WLR_LOG("[WLR] placeholder OnGLPresent fwd result=%{public}d",
              result ? 1 : 0);
    }
    return result;
  }
  uint32_t OnGLCreateFBO(int width, int height) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    return target_ ? target_->OnGLCreateFBO(width, height) : 0;
  }
  void* OnGLProcResolver(const char* name) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    return target_ ? target_->OnGLProcResolver(name) : nullptr;
  }

  // ---- Software / accelerated callbacks ----
  bool OnSoftwarePresent(const void* allocation, size_t row_bytes,
                         size_t height) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    return target_ ? target_->OnSoftwarePresent(allocation, row_bytes, height)
                   : false;
  }
  bool OnAcceleratedPresent() override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    return target_ ? target_->OnAcceleratedPresent() : false;
  }

  // ---- Platform task runner ----
  void OnPostTask(lynx_task_t task,
                  uint64_t interval_nanoseconds) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) {
      target_->OnPostTask(task, interval_nanoseconds);
    }
  }

  // ---- Platform IME / clipboard / cursor callbacks ----
  const char* GetClipboardData() override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    return target_ ? target_->GetClipboardData() : "";
  }
  void SetClipboardData(const char* data) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) target_->SetClipboardData(data);
  }
  void ActivateSystemCursor(lynx_cursor_type_e cursor_type,
                            const char* path) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) target_->ActivateSystemCursor(cursor_type, path);
  }
  void ShowTextInput(bool show) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) target_->ShowTextInput(show);
  }
  void UpdateCaretPosition(float x, float y, float width,
                           float height) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) target_->UpdateCaretPosition(x, y, width, height);
  }
  void SetCursorPosition(int position) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) target_->SetCursorPosition(position);
  }
  void SetMarkedTextRect(float x, float y, float width,
                         float height) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) target_->SetMarkedTextRect(x, y, width, height);
  }
  void SetEditableTransform(const float transform[16]) override {
    std::lock_guard<std::mutex> lock(target_mutex_);
    if (target_) target_->SetEditableTransform(transform);
  }

 private:
  using BoolMethod = bool (lynx::pub::LynxWindowlessRenderer::*)();
  bool Forward(BoolMethod method) {
    std::lock_guard<std::mutex> lock(target_mutex_);
    return target_ ? (target_.get()->*method)() : false;
  }

  mutable std::mutex target_mutex_;
  std::shared_ptr<lynx::pub::LynxWindowlessRenderer> target_;
  bool gl_fwd_logged_ = false;
  bool gl_present_logged_ = false;
};

std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
CreateHarmonyWindowlessRenderer(int32_t harmony_window_id,
                                void* egl_window,
                                int width,
                                int height) {
  // Must happen before the first LynxView is built, i.e. before anything can
  // call base::UIThread::Init() and claim the (never-pumped) fml loop.
  EnsureGlobalUiTaskRunner();
  std::lock_guard<std::mutex> lock(g_mutex);
  // ArkUI reuses the same OHNativeWindow across full-screen transitions. Keep
  // these dimensions current even when no renderer must be recreated; they
  // seed native-window geometry and must match incoming XComponent coordinates.
  g_surface_sizes[harmony_window_id] = {width, height};
  g_last_harmony_window_id = harmony_window_id;

  auto it = g_renderers.find(harmony_window_id);
  if (it != g_renderers.end() && it->second && it->second->window() == egl_window) {
    WLR_LOG("XComponent surface resized for id=%{public}d %{public}dx%{public}d",
            harmony_window_id, width, height);
    return it->second;
  }

  // Do NOT create EGL here (this runs on the surface-callback thread). EGL is
  // created lazily in OnGLMakeCurrent on Clay's GPU thread.
  auto renderer = std::make_shared<EglWindowlessRenderer>(egl_window);
  renderer->SetHarmonyWindowId(harmony_window_id);
  // InitIfNeeded() binds the capi callbacks and needs weak_from_this(), so it
  // must run after the shared_ptr owns the object.
  renderer->InitIfNeeded();
  g_renderers[harmony_window_id] = renderer;

  // If a placeholder renderer was created earlier by the LynxView build path,
  // bind the real renderer to it so the existing LynxView starts rendering.
  auto placeholder_it = g_placeholder_renderers.find(harmony_window_id);
  if (placeholder_it != g_placeholder_renderers.end()) {
    if (auto placeholder = placeholder_it->second.lock()) {
      auto* delegating =
          static_cast<DelegatingWindowlessRenderer*>(placeholder.get());
      delegating->SetTarget(renderer);
      WLR_LOG("Bound real renderer to placeholder for id=%{public}d",
              harmony_window_id);
    }
    g_placeholder_renderers.erase(placeholder_it);
  }

  WLR_LOG("Windowless GLDirect renderer created for id=%{public}d %{public}dx%{public}d "
          "(EGL deferred to gpu thread)", harmony_window_id, width, height);
  return renderer;
}

std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetHarmonyWindowlessRendererForWindow(int32_t harmony_window_id) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_renderers.find(harmony_window_id);
  if (it != g_renderers.end()) return it->second;
  return nullptr;
}

std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetCurrentHarmonyWindowlessRenderer() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return GetCurrentRendererLocked();
}

void CaptureHarmonyLynxPlatformTaskRunner() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_lynx_platform_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  WLR_LOG("Captured Lynx platform task runner");
}

bool GetHarmonySurfaceSizeForWindow(int32_t harmony_window_id, int* w, int* h) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_surface_sizes.find(harmony_window_id);
  if (it == g_surface_sizes.end()) return false;
  if (it->second.first <= 0 || it->second.second <= 0) return false;
  if (w) *w = it->second.first;
  if (h) *h = it->second.second;
  return true;
}

// Legacy single-window fallback: returns the most recent XComponent surface
// size (physical px) into *w,*h.
bool GetCurrentHarmonySurfaceSize(int* w, int* h) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_last_harmony_window_id > 0) {
    auto it = g_surface_sizes.find(g_last_harmony_window_id);
    if (it != g_surface_sizes.end()) {
      if (it->second.first > 0 && it->second.second > 0) {
        if (w) *w = it->second.first;
        if (h) *h = it->second.second;
        return true;
      }
    }
  }
  if (g_surface_sizes.empty()) return false;
  const auto& size = g_surface_sizes.begin()->second;
  if (size.first <= 0 || size.second <= 0) return false;
  if (w) *w = size.first;
  if (h) *h = size.second;
  return true;
}

bool GetHarmonyTextInputStateForWindow(int32_t harmony_window_id,
                                       float* x,
                                       float* y,
                                       float* width,
                                       float* height) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_renderers.find(harmony_window_id);
  if (it == g_renderers.end() || !it->second) return false;
  return it->second->GetTextInputState(x, y, width, height);
}

bool GetCurrentHarmonyTextInputState(float* x, float* y, float* width,
                                     float* height) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto renderer = GetCurrentRendererLocked();
  return renderer && renderer->GetTextInputState(x, y, width, height);
}

std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetOrCreateHarmonyPlaceholderRendererForWindow(int32_t harmony_window_id) {
  if (harmony_window_id <= 0) {
    return nullptr;
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  // If a real renderer already exists, there is no need for a placeholder.
  auto real_it = g_renderers.find(harmony_window_id);
  if (real_it != g_renderers.end() && real_it->second) {
    return nullptr;
  }
  auto placeholder_it = g_placeholder_renderers.find(harmony_window_id);
  if (placeholder_it != g_placeholder_renderers.end()) {
    if (auto existing = placeholder_it->second.lock()) {
      return existing;
    }
  }
  auto placeholder = std::make_shared<DelegatingWindowlessRenderer>();
  placeholder->InitIfNeeded();
  g_placeholder_renderers[harmony_window_id] = placeholder;
  WLR_LOG("Created placeholder renderer for id=%{public}d", harmony_window_id);
  return placeholder;
}

std::shared_ptr<lynx::pub::LynxWindowlessRenderer>
GetOrCreateHarmonyPlaceholderRendererForCppWindow(int32_t cpp_window_id) {
  if (cpp_window_id <= 0) {
    return nullptr;
  }
  std::lock_guard<std::mutex> lock(g_mutex);

  // If the ArkTS side already bound this C++ id to a HarmonyOS id, prefer the
  // real renderer (if the surface has arrived) or a harmony-keyed placeholder
  // so the surface callback can bind it without a later re-key. This covers
  // the race where LynxtronOnHarmonyWindowCreated ran BEFORE the view was
  // built (e.g. a slow first loadFile with a fast Ability startup).
  auto mapping = g_cpp_to_harmony.find(cpp_window_id);
  if (mapping != g_cpp_to_harmony.end()) {
    const int32_t harmony_id = mapping->second;
    auto real_it = g_renderers.find(harmony_id);
    if (real_it != g_renderers.end() && real_it->second) {
      WLR_LOG("cpp id=%{public}d already bound to harmony id=%{public}d with "
              "real renderer", cpp_window_id, harmony_id);
      return real_it->second;
    }
    auto pit = g_placeholder_renderers.find(harmony_id);
    if (pit != g_placeholder_renderers.end()) {
      if (auto existing = pit->second.lock()) {
        return existing;
      }
    }
    auto placeholder = std::make_shared<DelegatingWindowlessRenderer>();
    placeholder->InitIfNeeded();
    g_placeholder_renderers[harmony_id] = placeholder;
    WLR_LOG("Created harmony-keyed placeholder for cpp id=%{public}d "
            "(harmony id=%{public}d)", cpp_window_id, harmony_id);
    return placeholder;
  }

  // Binding has not happened yet: key by cpp id. BindHarmonyWindowIdForPlaceholder
  // will re-key it under the HarmonyOS id once the ArkTS side binds.
  auto it = g_cpp_placeholder_renderers.find(cpp_window_id);
  if (it != g_cpp_placeholder_renderers.end()) {
    if (auto existing = it->second.lock()) {
      return existing;
    }
  }
  auto placeholder = std::make_shared<DelegatingWindowlessRenderer>();
  placeholder->InitIfNeeded();
  g_cpp_placeholder_renderers[cpp_window_id] = placeholder;
  WLR_LOG("Created cpp-keyed placeholder renderer for cpp id=%{public}d",
          cpp_window_id);
  return placeholder;
}

void BindHarmonyWindowIdForPlaceholder(int32_t cpp_window_id,
                                       int32_t harmony_window_id) {
  if (cpp_window_id <= 0 || harmony_window_id <= 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mutex);

  // Always record the mapping, even if no cpp-keyed placeholder exists yet.
  // The view build path can run after this binding and must be able to resolve
  // the HarmonyOS id (or an already-created real renderer) without re-keying.
  g_cpp_to_harmony[cpp_window_id] = harmony_window_id;

  auto it = g_cpp_placeholder_renderers.find(cpp_window_id);
  if (it == g_cpp_placeholder_renderers.end()) {
    return;
  }
  auto placeholder = it->second.lock();
  if (!placeholder) {
    g_cpp_placeholder_renderers.erase(it);
    return;
  }
  // Re-key the pending placeholder under the HarmonyOS id so the surface
  // callback (CreateHarmonyWindowlessRenderer) can bind the real renderer.
  g_placeholder_renderers[harmony_window_id] = placeholder;
  g_cpp_placeholder_renderers.erase(it);
  WLR_LOG("Re-keyed placeholder renderer cpp id=%{public}d -> harmony id=%{public}d",
          cpp_window_id, harmony_window_id);
}

void ReleaseHarmonyWindowRenderer(int32_t cpp_window_id,
                                  int32_t harmony_window_id) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (harmony_window_id > 0) {
    g_renderers.erase(harmony_window_id);
    g_surface_sizes.erase(harmony_window_id);
    g_placeholder_renderers.erase(harmony_window_id);
  }
  if (cpp_window_id > 0) {
    g_cpp_placeholder_renderers.erase(cpp_window_id);
    g_cpp_to_harmony.erase(cpp_window_id);
  }
  WLR_LOG("Released renderer state cpp id=%{public}d harmony id=%{public}d",
          cpp_window_id, harmony_window_id);
}

void LynxtronSetHarmonyTextInputFocusCallback(
    HarmonyTextInputFocusCallback callback) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_text_input_focus_callback = callback;
}

}  // namespace lynxtron

// The NAPI bridge discovers this hook with dlsym(), which requires an
// unmangled C symbol.  Keep the implementation namespaced above while
// exporting this thin C-ABI entry point for the dynamic bridge.
extern "C" __attribute__((visibility("default")))
void LynxtronSetHarmonyTextInputFocusCallback(
    lynxtron::HarmonyTextInputFocusCallback callback) {
  lynxtron::LynxtronSetHarmonyTextInputFocusCallback(callback);
}

namespace {

// XComponent callbacks run on ArkUI's UIAbility thread, whereas the Lynx
// windowless renderer is owned by Lynxtron's UI runner.  Sending directly from
// ArkUI lets the call cross the C ABI but bypasses Clay's sequence-affinity
// assumptions, so focused editable controls never notify their text client.
void PostToLynxUi(base::OnceClosure task) {
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      lynxtron::LynxPlatformRunner();
  if (!runner) {
    runner = lynxtron::GetUIThreadTaskRunner();
  }
  if (runner) {
    runner->PostTask(FROM_HERE, std::move(task));
  }
}

// Builds a Lynx pointer event and dispatches it to the given window's renderer.
void SendPointerOnLynxUi(int32_t harmony_window_id,
                         int phase, double x, double y, int64_t buttons,
                         int32_t device, int kind, size_t timestamp) {
  auto r = harmony_window_id > 0
               ? lynxtron::GetHarmonyWindowlessRendererForWindow(harmony_window_id)
               : lynxtron::GetCurrentHarmonyWindowlessRenderer();
  if (!r) return;
  // Keep the event stream as small as the reference GLFW embedder does (a click
  // is just Down -> Up, never Add/Remove: Clay maps Remove to Cancel, which
  // undoes a completed tap). The one thing we must NOT leave zeroed is
  // device_kind: EmbedderEngine treats a zeroed device_kind as a legacy embedder
  // and reports the event as a MOUSE, so Lynx receives mousedown/mouseup and
  // `bindtap` -- which is part of the touch event family -- never fires.
  lynx_pointer_event_t ev = {};
  ev.struct_size = sizeof(ev);
  switch (phase) {
    case 0: ev.phase = kLynxPointerPhaseDown; break;
    case 1: ev.phase = kLynxPointerPhaseUp; break;
    case 2: ev.phase = kLynxPointerPhaseMove; break;
    case 4: ev.phase = kLynxPointerPhaseCancel; break;
    case 5: return;  // upstream embedders never emit Remove
    default: ev.phase = kLynxPointerPhaseHover; break;
  }
  ev.x = x;
  ev.y = y;
  ev.timestamp = timestamp;
  ev.device = device;
  ev.device_kind = static_cast<lynx_pointer_device_kind_e>(kind);
  // For touch, EmbedderEngine fills in the contact button itself; passing our
  // own mouse-style mask here would be wrong.
  if (kind != 2) {
    ev.buttons = buttons;
  }
  r->SendPointerEvent(&ev);
}

void SendScrollOnLynxUi(int32_t harmony_window_id, double x, double y,
                        double delta_x, double delta_y, size_t timestamp,
                        bool precise) {
  auto r = harmony_window_id > 0
               ? lynxtron::GetHarmonyWindowlessRendererForWindow(harmony_window_id)
               : lynxtron::GetCurrentHarmonyWindowlessRenderer();
  if (!r) return;
  lynx_pointer_event_t ev = {};
  ev.struct_size = sizeof(ev);
  ev.phase = kLynxPointerPhaseHover;
  ev.timestamp = timestamp;
  ev.x = x;
  ev.y = y;
  ev.device = 1;  // shared mouse device id used by the Harmony NAPI bridge
  ev.signal_kind = kLynxPointerSignalKindScroll;
  ev.scroll_delta_x = delta_x;
  ev.scroll_delta_y = delta_y;
  ev.device_kind = precise ? kLynxPointerDeviceKindTrackpad
                           : kLynxPointerDeviceKindMouse;
  ev.is_precise_scroll = precise ? 1 : 0;
  r->SendPointerEvent(&ev);
}

// Builds a Lynx key event and dispatches it to the given window's renderer.
void SendKeyOnLynxUi(int32_t harmony_window_id,
                     int type, uint64_t logical, uint64_t physical,
                     double timestamp) {
  auto r = harmony_window_id > 0
               ? lynxtron::GetHarmonyWindowlessRendererForWindow(harmony_window_id)
               : lynxtron::GetCurrentHarmonyWindowlessRenderer();
  if (!r) return;
  lynx_key_event_t ev = {};
  ev.struct_size = sizeof(ev);
  ev.timestamp = timestamp;
  ev.type = type == 0 ? kLynxKeyEventTypeUp : kLynxKeyEventTypeDown;
  ev.logical = logical;
  ev.physical = physical;
  ev.synthesized = false;
  r->SendKeyEvent(&ev);
}

// Builds a Lynx key event carrying IME text (composing or committed) and
// dispatches it to the given window's renderer.
void SendTextOnLynxUi(int32_t harmony_window_id,
                      std::string text, double timestamp, bool composing) {
  auto r = harmony_window_id > 0
               ? lynxtron::GetHarmonyWindowlessRendererForWindow(harmony_window_id)
               : lynxtron::GetCurrentHarmonyWindowlessRenderer();
  if (!r) {
    WLR_LOG("[IME] drop text: renderer unavailable");
    return;
  }
  WLR_LOG("[IME] dispatch %{public}s bytes=%{public}zu",
          composing ? "composing" : "committed", text.size());
  lynx_key_event_t ev = {};
  ev.struct_size = sizeof(ev);
  ev.timestamp = timestamp;
  // Repeat marks pre-edit text, Down marks a final commit — see the windowless
  // UI renderer's send_key_event bridge, which splits on exactly this.
  ev.type = composing ? kLynxKeyEventTypeRepeat : kLynxKeyEventTypeDown;
  ev.character = text.c_str();
  // IME text is generated by the platform input method rather than a physical
  // key. Clay uses this marker to route the character payload to the focused
  // text input (the macOS embedder's TypeText does the same).
  ev.synthesized = true;
  r->SendKeyEvent(&ev);
}

}  // namespace

// Exported for the NAPI bridge. Forwards an XComponent pointer/mouse event to
// the LynxView's windowless renderer so Lynx can hit-test and dispatch it.
//   phase: 0=down, 1=up, 2=move, 3=hover, 4=cancel, 5=remove
//   kind: 1=mouse, 2=touch, 3=stylus, 4=trackpad
extern "C" __attribute__((visibility("default"))) void LynxtronSendPointerEventForWindow(
    int32_t harmony_window_id,
    int phase, double x, double y, int64_t buttons, int32_t device, int kind,
    size_t timestamp) {
  PostToLynxUi(base::BindOnce(&SendPointerOnLynxUi, harmony_window_id,
                              phase, x, y, buttons, device, kind, timestamp));
}

extern "C" __attribute__((visibility("default"))) void LynxtronSendScrollEventForWindow(
    int32_t harmony_window_id, double x, double y, double delta_x,
    double delta_y, size_t timestamp, bool precise) {
  PostToLynxUi(base::BindOnce(&SendScrollOnLynxUi, harmony_window_id, x, y,
                              delta_x, delta_y, timestamp, precise));
}

extern "C" __attribute__((visibility("default"))) void LynxtronSendPointerEvent(
    int phase, double x, double y, int64_t buttons, int32_t device, int kind,
    size_t timestamp) {
  LynxtronSendPointerEventForWindow(-1, phase, x, y, buttons, device, kind,
                                    timestamp);
}

extern "C" __attribute__((visibility("default"))) void LynxtronSendKeyEventForWindow(
    int32_t harmony_window_id,
    int type, uint64_t logical, uint64_t physical, double timestamp) {
  PostToLynxUi(base::BindOnce(&SendKeyOnLynxUi, harmony_window_id,
                              type, logical, physical, timestamp));
}

extern "C" __attribute__((visibility("default"))) void LynxtronSendKeyEvent(
    int type, uint64_t logical, uint64_t physical, double timestamp) {
  LynxtronSendKeyEventForWindow(-1, type, logical, physical, timestamp);
}

extern "C" __attribute__((visibility("default"))) void LynxtronSendTextInputForWindow(
    int32_t harmony_window_id,
    const char* text, double timestamp) {
  if (!text || !*text) return;
  std::string committed_text(text);
  WLR_LOG("[IME] queue committed text bytes=%{public}zu", committed_text.size());
  PostToLynxUi(base::BindOnce(&SendTextOnLynxUi, harmony_window_id,
                              std::move(committed_text), timestamp,
                              /*composing=*/false));
}

extern "C" __attribute__((visibility("default"))) void LynxtronSendTextInput(
    const char* text, double timestamp) {
  LynxtronSendTextInputForWindow(-1, text, timestamp);
}

// Pre-edit (marked) text an IME is still composing. An empty string clears the
// composing region, which is how a cancelled composition is reported.
extern "C" __attribute__((visibility("default"))) void
LynxtronSendComposingTextForWindow(int32_t harmony_window_id,
                                   const char* text, double timestamp) {
  if (!text) return;
  std::string composing_text(text);
  WLR_LOG("[IME] queue composing text bytes=%{public}zu", composing_text.size());
  PostToLynxUi(base::BindOnce(&SendTextOnLynxUi, harmony_window_id,
                              std::move(composing_text), timestamp,
                              /*composing=*/true));
}

extern "C" __attribute__((visibility("default"))) void
LynxtronSendComposingText(const char* text, double timestamp) {
  LynxtronSendComposingTextForWindow(-1, text, timestamp);
}

extern "C" __attribute__((visibility("default"))) bool LynxtronGetTextInputStateForWindow(
    int32_t harmony_window_id,
    float* x, float* y, float* width, float* height) {
  return lynxtron::GetHarmonyTextInputStateForWindow(harmony_window_id,
                                                     x, y, width, height);
}

extern "C" __attribute__((visibility("default"))) bool LynxtronGetTextInputState(
    float* x, float* y, float* width, float* height) {
  return lynxtron::GetCurrentHarmonyTextInputState(x, y, width, height);
}

namespace {
std::mutex g_pending_mutex;
std::vector<std::string> g_pending_urls;
std::vector<std::string> g_pending_file_paths;

void DispatchOpenURL(const std::string& u) {
  if (auto* app = lynxtron::Application::Get()) {
    app->OpenURL(u);
  }
}

void DispatchOpenFile(const std::string& fp) {
  if (auto* app = lynxtron::Application::Get()) {
    app->OpenFile(fp);
  }
}

void DispatchQuit() {
  if (auto* app = lynxtron::Application::Get()) {
    app->Quit();
  }
}
}  // namespace

extern "C" __attribute__((visibility("default"))) void
LynxtronHandleOpenURL(const char* url) {
  if (!url || !*url) return;
  auto runner = lynxtron::GetUIThreadTaskRunner();
  if (runner) {
    runner->PostTask(FROM_HERE, base::BindOnce(&DispatchOpenURL,
                                               std::string(url)));
  } else {
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    g_pending_urls.emplace_back(url);
  }
}

extern "C" __attribute__((visibility("default"))) void
LynxtronHandleOpenPath(const char* file_path) {
  if (!file_path || !*file_path) return;
  auto runner = lynxtron::GetUIThreadTaskRunner();
  if (runner) {
    runner->PostTask(FROM_HERE, base::BindOnce(&DispatchOpenFile,
                                               std::string(file_path)));
  } else {
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    g_pending_file_paths.emplace_back(file_path);
  }
}

extern "C" __attribute__((visibility("default"))) void
LynxtronFlushPendingOpenURLs() {
  std::vector<std::string> urls;
  std::vector<std::string> file_paths;
  {
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    urls.swap(g_pending_urls);
    file_paths.swap(g_pending_file_paths);
  }
  for (const auto& u : urls) {
    DispatchOpenURL(u);
  }
  for (const auto& fp : file_paths) {
    DispatchOpenFile(fp);
  }
}

// Exported for the NAPI bridge. Mirrors electron_ohos's ExecuteCommandSingleton
// kAppQuit command: the OHOS system side (ArkTS/Ability) asks the browser to
// exit through the SAME graceful path (before-quit → close windows → ... →
// message loop exit), never a force-kill. The exit code is published by the
// bridge once LynxtronMain returns, and ArkUI then calls terminateSelf().
extern "C" __attribute__((visibility("default"))) void LynxtronQuit() {
  auto runner = lynxtron::GetUIThreadTaskRunner();
  if (runner) {
    runner->PostTask(FROM_HERE, base::BindOnce(&DispatchQuit));
  } else {
    WLR_ERR("LynxtronQuit: no UI thread task runner available");
  }
}
