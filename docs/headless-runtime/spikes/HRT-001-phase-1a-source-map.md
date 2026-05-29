# HRT-001: Phase 1a Source-Level Feasibility Map

## Status

Done.

Date: 2026-05-28

## Summary

The first macOS headless proof should start in the existing LynxWindow and
LynxView path, not in the SDK package. The smallest useful patch line is:

```text
process/headless flag
  -> harness-created LynxWindow/LynxView
  -> headless render-active override
  -> load fixture bundle
  -> trace ready-to-show/on-first-screen/error
  -> reuse DevTool touch simulation
  -> add screenshot backend or stop on missing capture API
```

The main known technical gap is screenshot capture from the Lynx render surface.
There are bitmap/PNG helpers in the tree, but no existing `LynxWindow` or
`LynxView` capture API was found.

## Source Map

| Area | Source | Finding | Phase 1a impact |
| --- | --- | --- | --- |
| macOS app target | `src/BUILD.gn:805` | `mac_app_bundle("lynxtron_app")` builds the macOS app. | HRT-002 should build against the existing app target first. |
| package copy target | `src/BUILD.gn:838` | `copy_app_to_package` copies `lynxtron.app` into package output. | Runtime binary packaging can remain later; first patch can use built app path. |
| unit test target | `src/BUILD.gn:1004` | `test("lynxtron_unittests")` includes native unit tests. | Input simulation regression can use existing unit target. |
| default app arg parse | `src/packages/default_app/main.ts:47` | `--app=` is parsed into `option.file`. | A dedicated headless flag can follow this argument parsing pattern. |
| default app loading | `src/packages/default_app/main.ts:79`, `:84`, `:248`-`:252`, `:285` | Current app loading is package-oriented, while file/url branches are commented. | Headless harness should probably be a separate harness entry or restore a bundle-file path deliberately. |
| default bundle window | `src/packages/default_app/default_app.ts:12`, `:21`-`:23` | Default app creates `LynxWindow`, calls `show()`, then `loadFile()`. | Do not reuse this directly for headless proof because it intentionally shows the window. |
| LynxWindow load APIs | `src/shell/api/api_lynx_window.cc:558`, `:598`, `:622`, `:948`-`:950` | Native methods expose `loadFile`, `loadURL`, and `loadBundle`. | HRT-002/HRT-004 can reuse existing load flow. |
| LynxView load implementation | `src/shell/api/lynx_view/lynx_view_impl.cc:180`, `:201`, `:217`, `:231` | File/url/bundle loading all end in `lynx_view_->LoadTemplate(...)`. | First proof can use file load with existing fixture bundle. |
| render active state | `src/shell/api/api_lynx_window.cc:307`-`:323` | `ComputeRenderActive()` requires `window_->IsVisible()`, and sync calls foreground/background. | HRT-003 must add explicit headless behavior instead of using only `show: false`. |
| visibility hooks | `src/shell/api/api_lynx_window.cc:549`, `:554` | show/hide events call `SyncRenderActiveState()`. | Headless mode should enter foreground without visible-window show. |
| first-screen events | `src/shell/api/api_lynx_window.cc:832`, `:839` | Load success emits `ready-to-show`; first screen emits `on-first-screen`. | HRT-004 can trace these events directly from LynxWindow. |
| error event path | `src/shell/api/api_lynx_window.cc:849`-`:853`, `:891`-`:900` | Lynx errors are routed through `ReportErrorToNode()` and emitted as `receive-lynx-window-error`. | HRT-004 should normalize this into `trace.jsonl` and `report.json`. |
| frame timing | `src/shell/api/api_lynx_window.cc:510`, `:537`, `:545` | Existing FPS monitor emits `frame-timings`. | Optional for Phase 1a; useful for later stable-frame wait. |
| LynxView wrapper | `src/shell/api/lynx_view/lynx_view.h:31`-`:49` | Public wrapper exposes load, data, global event, screen metrics, frame, foreground/background. | Headless control can stay near this wrapper or `LynxWindow`. |
| LynxView implementation | `src/shell/api/lynx_view/lynx_view_impl.h:38`-`:56` | Impl exposes load, bounds, screen metrics, native window, foreground/background. | HRT-003 may need to thread headless surface state through this layer. |
| input simulation install | `src/shell/api/lynx_view/lynx_view_impl.cc:165`-`:167` | `SetEventSimulationProxy(...)` is installed during initialization. | HRT-006 can reuse this path for tap/wheel. |
| hit testing | `src/shell/api/lynx_view/devtool_event_simulation_proxy.cc:24`-`:35` | Event target calls `GetNodeForLocation()` and `SendTouchEvent()`. | Tap proof depends on headless hit testing returning a valid tag. |
| touch simulation | `src/shell/api/lynx_view/devtool_event_simulation_proxy.cc:53`-`:103` | Mouse press/move/release become touchstart/touchmove/touchend/tap. | CDP `Input.dispatchTouchEvent` can map to this implementation. |
| input unit tests | `src/shell/api/lynx_view/devtool_event_simulation_proxy_unittest.cc:75`-`:168` | Tests cover tap, invalid target, right mouse, and wheel. | HRT-006 should extend or reuse these tests before runtime smoke. |
| WebContents capture absence | `src/lib/browser/api/lynx-window.ts:199`-`:258` | WebContents-style APIs, including `capturePage`, are commented placeholders. | HRT-005 cannot rely on Electron-style `webContents.capturePage()`. |
| bitmap to PNG utility | `src/shell/api/api_native_image.cc:250`-`:253` | `NativeImage::ToPNG()` encodes a `SkBitmap` with `gfx::PNGCodec`. | Useful only after a render-surface bitmap is available. |
| SkCanvas pixel helper | `src/shell/ui/skia/ext/platform_canvas.h:99`-`:104` | `ReadPixels(SkCanvas*)` exists for SkCanvas. | Possible utility, but no link to Lynx render surface found yet. |
| JS spec fixtures | `src/spec/api-lynx-window-template-api-spec.ts:78`, `:109`, `:149` | Existing specs load real `.lynx.bundle` through file/url/bundle. | HRT-004 fixture can reuse these bundles if available. |
| bridge fixture | `src/spec/bridging-lynx-node-spec.ts:38`, `:41`, `:88`, `:91` | Existing specs observe `-lynx-invoke` and `-lynx-message`. | Useful later for trace and JS mock work. |
| BTS preload fixture | `src/spec/lynx-node-bts-await-spec.ts:24`-`:31`, `:44` | Existing spec uses `lynxPreference.preload` and `sendGlobalEvent`. | Useful later for JS mock injection validation. |
| npm test scripts | `src/package.json:32`, `:33` | `npm test` runs `script/spec-runner.js`; `test:build:lynx` builds Lynx spec bundle. | SDK/JS smoke can reuse existing scripts after runtime proof exists. |

## Investigation Answers

### 1. Headless Flag Entry

Most likely entry points:

- Runtime process argument parsing in `src/packages/default_app/main.ts`.
- A new dedicated harness app under `src/packages/`.
- Native command-line state under `src/shell/common/lynxtron_command_line.*`
  if the mode must be visible before JS app bootstrap.

Recommendation:

- For Phase 1a, use an explicit process flag such as `--headless-runtime` and
  route it to a dedicated harness entry.
- Avoid modifying the default visible app behavior beyond shared argument
  parsing.

### 2. Harness Reuse Or Separate App

The existing default app is not ideal as-is:

- `default_app.ts` calls `mainWindow.show()`.
- `main.ts` currently routes arbitrary paths to `loadApplicationPackage(file)`;
  file/url loading branches are commented.

Recommendation:

- Add a separate headless harness entry for Phase 1a.
- Reuse the existing `LynxWindow` load APIs, not the visible default app flow.

### 3. Render Active Ownership

Current owner:

- `LynxWindow::ComputeRenderActive()`.
- `LynxWindow::SyncRenderActiveState()`.

Problem:

- `ComputeRenderActive()` requires `window_->IsVisible()`.
- A hidden window therefore enters background rather than foreground.

Recommendation:

- Add a headless session state bit near `LynxWindow`.
- In headless mode, `ComputeRenderActive()` should treat an existing,
  non-closed window/view as render active even when no user-visible window is
  shown.
- Keep normal visible app behavior unchanged.

### 4. Screenshot Capture

Known source:

- `NativeImage::ToPNG()` can encode a bitmap.
- `skia::ReadPixels(SkCanvas*)` can copy pixels from a canvas.

Missing source:

- No current `LynxWindow.capturePage`.
- No current `LynxView` method found that exposes rendered pixels or a canvas.

Recommendation:

- Treat screenshot as the highest-risk Phase 1a item.
- First search deeper in Lynx public/embedder APIs for a render snapshot,
  surface, canvas, or texture readback API.
- If no API exists, HRT-005 should introduce a narrow native screenshot method
  at the LynxView/render-surface boundary and encode PNG using existing helpers.

### 5. Input Simulation

The input path is strong enough for Phase 1a tap proof:

- `SetEventSimulationProxy()` is installed during `LynxViewImpl::Initialize()`.
- Press/move/release converts to touch events and tap when movement is within
  slop.
- Existing unit tests cover tap and wheel behavior.

Risk:

- Headless hit testing must still return a valid node tag through
  `GetNodeForLocation()`.

Recommendation:

- HRT-006 should first verify headless hit testing, then wire the protocol
  method to the existing proxy.

### 6. Event Trace

Minimal trace source:

- `ready-to-show` from `LynxWindow::OnLoadSuccess()`.
- `on-first-screen` from `LynxWindow::OnFirstScreen()`.
- `receive-lynx-window-error` from `ReportErrorToNode()`.
- Optional `frame-timings` from the FPS monitor.

Recommendation:

- HRT-004 should implement trace/report generation in the harness before full
  protocol work.

### 7. Fixture Bundle

Candidate fixtures:

- `src/spec/case/lynx-card/dist/bridging-lynx-node.lynx.bundle`
- `src/spec/case/lynx-card/dist/lynx-node-bts-await.lynx.bundle`
- `src/spec/case/lynx-card/dist/contextbridge-lynx-node.lynx.bundle`

Build helper:

- `src/package.json:33` defines `test:build:lynx`.

Recommendation:

- Use an existing bundle for initial load/first-screen proof.
- Add a smaller tap-specific fixture only when HRT-006 starts.

### 8. Build And Launch

Known targets:

- `src:lynxtron_app` for the macOS app.
- `src:lynxtron_unittests` for unit tests.

Likely local commands:

```bash
ninja -C out/Debug lynxtron_app
ninja -C out/Debug lynxtron_unittests
```

Known local runtime shape from existing approved commands:

```bash
out/Debug/lynxtron.app/Contents/MacOS/lynxtron
```

These commands should be confirmed against the developer's configured output
directory before implementation starts.

## Proposed Follow-Up Tasks

### HRT-002: Headless Harness Entry

Scope:

- Add explicit headless runtime flag.
- Add dedicated harness entry or minimal branch that creates a controlled
  `LynxWindow` without showing the default app UI.

Primary files:

- `src/packages/default_app/main.ts`
- Possible new files under `src/packages/headless_runtime/`
- `src/BUILD.gn`

Verification:

- Build `lynxtron_app`.
- Launch headless flag with a fixture bundle path.
- Trace process startup and bundle load request.

### HRT-003: Headless Render Active Mode

Scope:

- Add headless state to the LynxWindow/LynxView path.
- Decouple headless render active from `window_->IsVisible()`.
- Preserve visible-app behavior.

Primary files:

- `src/shell/api/api_lynx_window.h`
- `src/shell/api/api_lynx_window.cc`
- Potentially `src/shell/api/lynx_view/lynx_view.*`

Verification:

- Native/unit coverage for render active computation if practical.
- Runtime trace shows foreground state before first screen in headless mode.

### HRT-004: First-Screen Trace And Report Skeleton

Scope:

- Capture load start, `ready-to-show`, `on-first-screen`, and Lynx errors.
- Write `trace.jsonl` and `report.json`.

Primary files:

- Headless harness files from HRT-002.
- `src/shell/api/api_lynx_window.cc` only if additional event plumbing is
  needed.

Verification:

- Launch with existing fixture bundle.
- Confirm trace and report contain expected events and stable status.

### HRT-005: Screenshot Backend

Scope:

- Identify or add a narrow rendered-pixel capture API for LynxView.
- Encode PNG artifact.

Primary files:

- `src/shell/api/lynx_view/*`
- Possible Lynx embedder/render surface integration files after deeper search.
- Existing image helpers only for encoding.

Verification:

- Launch fixture and produce non-empty PNG.
- Add failure path into report/trace.

### HRT-006: Tap Smoke

Scope:

- Route one input action to existing DevTool event simulation.
- Verify tap changes fixture state or emits a traceable event.

Primary files:

- Headless protocol/harness files from HRT-002/HRT-004.
- `src/shell/api/lynx_view/devtool_event_simulation_proxy.*` only if the
  existing proxy needs a small extension.

Verification:

- Existing native unit target still passes for event simulation.
- Runtime smoke dispatches tap and observes expected fixture effect.

## Verification Commands Run

```bash
rg -n "ComputeRenderActive|IsVisible|OnFirstScreen|ready-to-show|on-first-screen|frame-timings|ReportErrorToNode|SetFpsMonitorEnabled|EmitFpsEvent" src/shell/api src/lib/browser src/packages
rg -n "SetEventSimulationProxy|DevToolEventSimulationProxy|GetNodeForLocation|EmulateTouch|Dispatch|touch|mouse|wheel" src/shell/api/lynx_view
rg -n "capture|Capture|screenshot|Screenshot|snapshot|Snapshot|ReadPixels|readPixels|SkBitmap|bitmap|Bitmap|surface" src/shell src/lib src/packages
rg -n -e "--app|loadApplication|loadBundle|loadFile|loadURL|LynxTemplateBundle|default bundle|lynx\\.bundle" src/packages/default_app src/packages/lynxtron-shell-demo src/spec src/packages/lynxtron/apis
rg -n "GetNodeForLocation|SendTouchEvent|EmulateMouseEvent|UpdateData|UpdateMetaData|LoadTemplate|LoadBundle|LoadFile|LoadUrl|SetRenderActive|render_active|SetGlobalProps|SendGlobalEvent" src/shell/api/api_lynx_window.cc src/shell/api/lynx_view src/packages/lynxtron/apis src/lib/browser/api
```

Supporting source reads:

```bash
sed -n '280,335p' src/shell/api/api_lynx_window.cc
sed -n '820,905p' src/shell/api/api_lynx_window.cc
sed -n '150,175p' src/shell/api/lynx_view/lynx_view_impl.cc
sed -n '20,110p' src/shell/api/lynx_view/devtool_event_simulation_proxy.cc
sed -n '520,665p' src/shell/api/api_lynx_window.cc
sed -n '180,240p' src/shell/api/lynx_view/lynx_view_impl.cc
sed -n '35,95p' src/packages/default_app/main.ts
sed -n '240,292p' src/packages/default_app/main.ts
sed -n '1,80p' src/packages/default_app/default_app.ts
sed -n '1,115p' src/spec/api-lynx-window-template-api-spec.ts
sed -n '1,75p' src/spec/lynx-node-bts-await-spec.ts
sed -n '1,105p' src/spec/bridging-lynx-node-spec.ts
sed -n '90,110p' src/shell/ui/skia/ext/platform_canvas.h
sed -n '245,260p' src/shell/api/api_native_image.cc
sed -n '1,70p' src/shell/api/lynx_view/lynx_view_impl.h
sed -n '1,80p' src/shell/api/lynx_view/lynx_view.h
```

## Blockers And Risks

No blocker for the PM/design artifact.

Implementation risks:

- Screenshot is not yet mapped to an existing LynxView/render-surface API.
- macOS may still require a non-visible native host even if the user-visible
  window is avoided.
- Headless hit testing must be verified before tap can be accepted.
- The exact output directory and build command should be confirmed in the local
  build environment before HRT-002 starts.

## Recommendation

Start implementation with HRT-002 and HRT-003 before SDK/CLI work. Do not spend
time on public API polish until a headless process can load a fixture and reach
`on-first-screen` without showing a user-visible window.
