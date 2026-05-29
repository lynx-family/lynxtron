# Lynxtron Headless Runtime Engineering Design

## Status

MVP implementation accepted for the macOS no-image CDP slice. Broader v1 work
for Linux, image-heavy bundles, JS NativeModules mock loading, public locators,
and adapters remains pending.

Date: 2026-05-28

## Product Position

Build a true headless Lynxtron runtime that can run real Lynx bundles in CI,
cloud, and AI/tooling environments. The product API is Lynx-aware; the UI
inspection and interaction control plane is CDP-compatible.

The runtime is not a standalone Node native addon. The SDK and CLI drive a
headless Lynxtron binary through a protocol endpoint.

## Confirmed Decisions

1. Add an independent CDP-compatible gateway inside the headless runtime. Do not
   depend on the GUI LynxDevTool session model.
2. v1 supports only harness-created bundle sessions. It does not attach to an
   arbitrary user Lynxtron app.
3. v1 must prove true headless runtime capability. It is not enough to wrap an
   existing GUI Lynxtron app with JS and CDP.
4. Platform sequence: macOS prototype first, Linux CI/cloud next, Windows out of
   scope for v1.
5. Virtual environments must be fully customizable.
6. NativeModules mocks are JS-only. Do not build a JSON mock DSL.
7. Publicly expose a CDP-compatible WebSocket endpoint for advanced integration,
   but do not claim full Chrome CDP compatibility.
8. CLI is for runtime smoke, artifacts, reports, and debug entry points. It is
   not a test framework.
9. SDK provides operation and observation APIs only. It does not provide
   assertions, a runner, or reporters.
10. v1 locators use Lynx semantics: text, testId, and basic attributes. Do not
    promise full CSS selector, XPath, role locator, or React component locator.
11. MVP artifacts are screenshots, before/after UI dumps, `report.json`,
    `trace.jsonl`, and `replay.json`.
12. Public errors are structured. CLI exit codes are fixed.
13. `on-first-screen` is the default success/screenshot wait signal.
    `ready-to-show` is an intermediate load signal.
14. v1 supports one active session per runtime process. Parallelism is achieved
    by launching multiple runtime processes.
15. v1 ships as one npm package, `@lynx-js/lynxtron-headless`, with SDK, CLI,
    protocol client, type declarations, and runtime binary resolution.

## Goals

- Run a Lynx bundle in true headless Lynxtron.
- Provide a Node SDK for lifecycle, loading, virtual environment setup, UI
  observation, UI actions, artifacts, and diagnostics.
- Provide a CLI for CI smoke checks and artifact generation.
- Provide a CDP-compatible WebSocket endpoint for UI observation and operation.
- Provide Lynx custom protocol domains for Lynx runtime, device profile,
  NativeModules JS mocks, bridge tracing, lifecycle, and artifact access.
- Prove the end-to-end chain on macOS first, then make Linux CI/cloud the v1
  product acceptance target.

## Non-Goals

- No Playwright adapter in v1.
- No MCP tool/server in v1.
- No test runner, assertion library, snapshot diff, or visual regression system.
- No arbitrary Lynxtron app attach/session in v1.
- No independent Node native addon in v1.
- No built-in standard iOS/Android NativeModules mock library.
- No JSON mock DSL, now or later.
- No Windows v1 acceptance.

## Hypothetical Consumers

Playwright adapters, MCP tools, and AI skills are not shipped in v1, but the API
must remain usable by them.

Design constraints from these hypothetical consumers:

- Expose `wsEndpoint()` as a stable advanced integration point.
- Keep protocol messages deterministic and machine-readable.
- Keep UI operation and observation APIs separate from assertion/reporting
  concerns.
- Return structured errors with stable codes.
- Produce artifacts in ordinary files, not only process stdout.
- Preserve enough locator and snapshot data for an adapter layer to map its own
  higher-level concepts later.

## Current Source Baseline

| Capability | Existing source evidence | Design impact |
| --- | --- | --- |
| LynxWindow can be created with size, visibility, and preload options | `src/packages/lynxtron/apis/api/lynx-window.d.ts:20`, `:24`, `:28`, `:128`, `:244` | Harness can create controlled windows and inject preload/mock code. |
| Loading Lynx bundle/file/url is already public | `src/packages/lynxtron/apis/api/lynx-window.d.ts:342`, `:359`, `:373`; native binding at `src/shell/api/api_lynx_window.cc:948`-`:950` | Bundle session should reuse `loadFile`, `loadURL`, and `loadBundle`. |
| Data/globalProps update APIs exist | `src/packages/lynxtron/apis/api/lynx-window.d.ts:390`, `:398`; native binding at `src/shell/api/api_lynx_window.cc:951`-`:952` | SDK can expose `data` and `globalProps` at load time and update time. |
| Global event dispatch exists | `src/packages/lynxtron/apis/api/lynx-window.d.ts:404`; native binding at `src/shell/api/api_lynx_window.cc:953` | Custom lifecycle and test events can reuse this path where appropriate. |
| Load and first-screen events exist | `src/shell/api/api_lynx_window.cc:832`, `:839` | Default wait strategy should use `on-first-screen`; `ready-to-show` is intermediate. |
| Error event path exists | `src/shell/api/api_lynx_window.cc:238`, `:849`-`:853`, `:891`-`:900` | Runtime can convert Lynx load/runtime errors to structured protocol/SDK errors. |
| Bridge call/send is already observable | API types at `src/packages/lynxtron/apis/api/lynx-window.d.ts:269`-`:270`; native module emits at `src/shell/api/lynx_view/module/lynx_bridge_module.cc:63`, `:82` | Bridge trace and reply handling can be implemented in the harness. |
| `contextBridge.exposeInLynxBTS` exists | `src/packages/lynxtron/apis/api/context-bridge.d.ts:6`; BTS implementation in `src/lib/lynxbts/init.ts:28`, `:39`, `:82` | JS NativeModules mocks can use the existing injection model. |
| Web symmetric host has a NativeModules simulation model | `src/packages/lynxtron/web-host/index.js:33`, `:34`, `:42`, `:91`, `:96`, `:97` | Headless mock design should copy the JS-first model, not invent a JSON DSL. |
| Default app already parses application launch inputs | `src/packages/default_app/main.ts:47`, `:79`, `:84`, `:272`, `:284`-`:285` | The headless harness can follow the existing bundle/app loading shape, while keeping v1 limited to harness-created bundle sessions. |
| Template bundle predecode wrapper exists | `src/packages/lynxtron/apis/api/lynx-template-bundle.d.ts:5`, `:24`, `:29`; native constructor at `src/shell/api/api_lynx_template_bundle.cc:50`-`:74` | SDK can support buffer/template bundle inputs after file/url are stable. |
| App window creation can be observed | `src/lib/browser/api/lynx-window.ts:149`; API type in `src/packages/lynxtron/apis/api/app.d.ts:467` | Useful for future app attach, but v1 does not expose arbitrary app sessions. |
| DevTool input simulation already exists | `src/shell/api/lynx_view/devtool_event_simulation_proxy.cc:53`-`:104`; installed in `src/shell/api/lynx_view/lynx_view_impl.cc:165`-`:167` | CDP `Input.dispatchTouchEvent` can reuse this path. |
| Render active currently depends on visible window | `src/shell/api/api_lynx_window.cc:307`-`:309`; visibility sync at `:312`-`:313`, `:549`, `:554` | True headless requires runtime work, not only `show: false`. |
| WebContents-style APIs are only commented placeholders | `src/lib/browser/api/lynx-window.ts:199`-`:258` | Screenshot/DOM/CSS/CDP cannot rely on Electron WebContents APIs. |

## Current MVP Implementation Snapshot

The accepted MVP keeps native UI inspection behind the CDP control plane:

- CDP server: `src/packages/lynxtron-headless/harness/cdp-server.js`
- CDP client: `src/packages/lynxtron-headless/lib/cdp-client.js`
- SDK run/replay entry points: `src/packages/lynxtron-headless/lib/index.js`
- CLI run/record/replay entry points: `src/packages/lynxtron-headless/cli.js`
- Headless harness and implemented methods:
  `src/packages/lynxtron-headless/harness/main.js`
- macOS observed-input provider:
  `src/shell/api/ui/mac/lynx_ns_window.mm`,
  `src/shell/app/native_window_observer.h`, and
  `src/shell/api/api_base_window.cc`
- Internal JS bridge from runtime to native dump backing:
  `src/shell/api/api_lynx_window.cc`
- Native windowless UI dump backing for `Lynx.dumpUITree`:
  `lynx/platform/embedder/windowless/lynx_ui_renderer_windowless.cc`
- Native C API and wrappers:
  `lynx/platform/embedder/public/capi/lynx_view_capi.h`,
  `lynx/platform/embedder/lynx_view.cc`,
  `lynx/platform/embedder/public/lynx_view.h`,
  `src/shell/api/lynx_view/lynx_view_impl.cc`, and
  `src/shell/api/lynx_view/lynx_view.cc`
- Complex no-image acceptance fixture:
  `src/packages/lynxtron-headless/fixtures/complex-app/`

The internal native method name `__dumpHeadlessUITreeForCDP` is intentionally
not a durable public SDK API. The stable observation path for the MVP is
`Lynx.dumpUITree` over the runtime-local CDP-compatible WebSocket endpoint.

`trace.jsonl` is the diagnostic timeline. `replay.json` is the machine
reproduction contract. It records source hash, runtime metadata, device profile,
load options, CDP action params, semantic tap target, result, and artifact
hashes. The CLI supports semantic replay, which re-finds the target from the UI
dump, exact replay, which reuses the recorded coordinates, and recorded replay,
which replays recorded `Input.dispatchTouchEvent` actions from the manifest.

Headed authoring mode is a debug mode on top of the same windowless/CDP control
path. `--headed` shows a visible host window and `--slow-mo` delays key CDP
actions so a developer can watch the automation flow. Reports identify this
mode as `authoring.rendererMode: windowless-renderer-visible-host`.

Input monitoring is also exposed through CDP, not as a durable native SDK API.
The accepted domain is `LynxRecorder.*`. Providers emit a normalized observed
input event; the harness broadcasts `LynxRecorder.inputEvent`, optionally
forwards it into the headless renderer as CDP input, captures UI snapshots, and
writes the generated actions into `replay.json`. The current provider set is:

- `macos-lynxtron`: native `NSWindow.sendEvent` capture in Lynxtron.
- `cdp-provider`: generic injection through
  `LynxRecorder.dispatchObservedInputEvent`, used by tests and suitable for a
  future mobile bridge.

## Architecture

```text
@lynx-js/lynxtron-headless
  - Node SDK
  - CLI: lynxtron-headless
  - protocol client
  - runtime binary resolver
        |
        | launch process + WebSocket protocol
        v
Headless Lynxtron Binary
  - headless harness app
  - session controller
  - CDP-compatible gateway
  - Lynx custom protocol domains
        |
        v
Headless Lynx Runtime Surface
  - LynxWindow/LynxView lifecycle
  - true headless render backend
  - screenshot backend
  - DevTool input simulation bridge
  - DOM/CSS/snapshot inspection bridge
  - JS NativeModules mock injection
  - bridge and NativeModules trace collector
```

The public SDK should be Lynx-oriented. CDP is the protocol control plane, not
the product abstraction.

## Runtime Process Model

v1 launches one Lynxtron headless process per active session.

```text
SDK/CLI
  -> resolve headless Lynxtron binary
  -> spawn runtime with harness entry
  -> wait for gateway ready
  -> create session
  -> load bundle/url/template bundle
  -> wait for on-first-screen
  -> operate/inspect/screenshot
  -> collect artifacts
  -> close process
```

Concurrency is external. CI can run multiple processes in parallel.

## True Headless Requirements

`headless: true` must not mean only `show: false`.

Minimum v1 runtime work:

- Introduce an explicit headless runtime mode.
- Create a render surface that does not require a user-visible OS window.
- Decouple render active state from `window_->IsVisible()` when the session is
  running in headless mode.
- Ensure load, layout, first-screen, screenshot, input hit testing, and node
  inspection work against the headless surface.
- Provide a screenshot path that does not depend on Electron WebContents.
- Preserve a platform abstraction so macOS prototype code can lead to Linux
  CI/cloud support.

Implementation phases:

1. macOS prototype:
   - Add headless mode flag and harness launch path.
   - Make headless sessions render active without a visible user window.
   - Produce screenshot from the Lynx render surface.
   - Reuse DevTool event simulation for tap/scroll input.
   - Expose minimal snapshot/box/style data through the gateway.
2. Linux backend:
   - Port the headless render surface.
   - Validate on Linux CI/cloud without a visible desktop session.
   - Stabilize fonts, DPR, locale, and screenshot determinism.

## SDK API

Package: `@lynx-js/lynxtron-headless`

Example:

```ts
import { launch } from '@lynx-js/lynxtron-headless';

const runtime = await launch({
  headless: true,
  artifactDir: './artifacts',
});

const session = await runtime.createSession({
  device: {
    platform: 'ios',
    viewport: { width: 390, height: 844 },
    dpr: 3,
    safeArea: { top: 59, right: 0, bottom: 34, left: 0 },
    orientation: 'portrait',
    locale: 'zh-CN',
  },
  nativeModules: './mocks/native-modules.js',
});

const page = await session.loadBundle('./dist/main.lynx.bundle', {
  data: {},
  globalProps: {},
});

await page.waitForFirstScreen();
await page.getByText('Login').tap();

await page.screenshot({ path: './artifacts/home.png' });
const report = await page.report();

await runtime.close();
```

Core SDK objects:

- `Runtime`
  - `wsEndpoint(): string`
  - `createSession(options): Promise<Session>`
  - `close(): Promise<void>`
- `Session`
  - `loadBundle(pathOrBuffer, options): Promise<Page>`
  - `loadURL(url, options): Promise<Page>`
  - `setDeviceProfile(profile): Promise<void>`
  - `setNativeModules(mockObjectOrModulePath): Promise<void>`
  - `close(): Promise<void>`
- `Page`
  - `waitForReadyToShow(options): Promise<void>`
  - `waitForFirstScreen(options): Promise<void>`
  - `snapshot(options): Promise<LynxSnapshot>`
  - `screenshot(options): Promise<ScreenshotArtifact>`
  - `locator(query): Locator`
  - `getByText(text): Locator`
  - `getByTestId(testId): Locator`
  - `consoleMessages(): Promise<ConsoleMessage[]>`
  - `errors(): Promise<HeadlessError[]>`
  - `bridgeCalls(): Promise<TraceEvent[]>`
  - `nativeModuleCalls(): Promise<TraceEvent[]>`
  - `report(): Promise<SessionReport>`
- `Locator`
  - `tap(options): Promise<ActionResult>`
  - `fill(value, options): Promise<ActionResult>`
  - `scrollIntoView(options): Promise<ActionResult>`
  - `boundingBox(): Promise<Rect | null>`
  - `innerText(): Promise<string>`
  - `computedStyle(properties?): Promise<Record<string, string>>`

The SDK must not expose `test`, `expect`, snapshot assertions, retries, or
reporter UI.

## CLI API

Binary: `lynxtron-headless`

Primary command:

```bash
lynxtron-headless run ./dist/main.lynx.bundle \
  --device ./device/iphone15.js \
  --native-modules ./mocks/native-modules.js \
  --data ./fixtures/data.json \
  --global-props ./fixtures/global-props.json \
  --screenshot ./artifacts/home.png \
  --report ./artifacts/report.json \
  --trace ./artifacts/trace.jsonl
```

Debug endpoint:

```bash
lynxtron-headless run ./dist/main.lynx.bundle --inspect-headless=0
```

CLI responsibilities:

- Validate input paths and config.
- Launch one runtime process.
- Load bundle/url.
- Wait for `on-first-screen` by default.
- Save screenshot/report/trace artifacts.
- Print the WebSocket endpoint when requested.
- Exit with a stable code.

CLI non-responsibilities:

- No test runner.
- No assertion DSL.
- No snapshot diff.
- No visual regression report.
- No Playwright adapter.
- No MCP server.

## Protocol Design

The runtime exposes a WebSocket endpoint using JSON-RPC/CDP-style messages.

### Current MVP Implemented Methods

The accepted MVP implements this protocol subset:

- `Lynx.loadBundle`
- `Lynx.loadURL`
- `Lynx.waitForReadyToShow`
- `Lynx.waitForFirstScreen`
- `Lynx.waitForFrame`
- `Lynx.waitForFrameAfter`
- `Lynx.dumpUITree`
- `Lynx.getHeadlessMetrics`
- `Page.takeScreenshot`
- `Input.dispatchTouchEvent`
- `LynxRecorder.start`
- `LynxRecorder.stop`
- `LynxRecorder.getEvents`
- `LynxRecorder.dispatchObservedInputEvent`

The accepted complex smoke uses this path:

```text
Lynx.loadBundle
  -> Lynx.waitForReadyToShow
  -> Lynx.waitForFirstScreen
  -> Page.takeScreenshot
  -> Lynx.dumpUITree
  -> Input.dispatchTouchEvent
  -> Lynx.waitForFrameAfter
  -> Lynx.dumpUITree
```

### Planned CDP-Compatible UI Domains

The broader v1 design may add these methods. Full Chrome CDP compatibility is
not a goal.

- `Page.takeScreenshot`
- `Runtime.consoleAPICalled`
- `Runtime.exceptionThrown`
- `DOM.getDocument`
- `DOM.querySelector`
- `DOM.querySelectorAll`
- `DOM.getBoxModel`
- `DOM.innerText`
- `CSS.getComputedStyleForNode`
- `Input.dispatchTouchEvent`
- `Input.dispatchKeyEvent`
- `Input.dispatchMouseEvent` for wheel/right-click compatibility when mapped to
  the existing DevTool event simulation proxy.

### Lynx Custom Domains

- `Lynx.loadBundle`
- `Lynx.loadURL`
- `Lynx.waitForReadyToShow`
- `Lynx.waitForFirstScreen`
- `Lynx.getLoadErrors`
- `Lynx.getFrameTimings`
- `Lynx.snapshot`
- `Lynx.dispatchGlobalEvent`
- `LynxDevice.setProfile`
- `LynxLifecycle.dispatch`
- `LynxNativeModules.setMockModule`
- `LynxNativeModules.getCalls`
- `LynxBridge.getCalls`
- `LynxBridge.reply`
- `LynxArtifacts.getReport`
- `LynxArtifacts.getTrace`

### Recorder Domain

`LynxRecorder` is a custom CDP-compatible domain for provider-neutral input
observation and recording.

Implemented methods:

- `LynxRecorder.start({ forwardToRuntime, captureUISnapshots })`
- `LynxRecorder.stop({ reason })`
- `LynxRecorder.getEvents()`
- `LynxRecorder.dispatchObservedInputEvent(event)`

Implemented events:

- `LynxRecorder.recordingStarted`
- `LynxRecorder.inputEvent`
- `LynxRecorder.actionRecorded`
- `LynxRecorder.uiSnapshot`
- `LynxRecorder.recordingStopped`
- `LynxRecorder.recordingError`

Observed input event shape:

```json
{
  "provider": "macos-lynxtron",
  "kind": "pointer",
  "type": "mousePressed",
  "pointerType": "mouse",
  "deviceKind": "mouse",
  "coordinateSpace": "viewport",
  "x": 195,
  "y": 611,
  "insideContent": true,
  "button": "left",
  "buttons": 1
}
```

Mobile recording should plug in by emitting this schema over the same CDP
domain. It must not require a separate manifest format.

## Locator And Snapshot Model

v1 locator scope:

- text
- test id
- basic attributes
- node id returned from snapshots

v1 must not promise:

- full CSS selector compatibility
- XPath
- accessibility role locator
- React component tree locator

Snapshot result should be machine-readable:

```ts
type LynxSnapshotNode = {
  nodeId: number;
  type: string;
  text?: string;
  attributes?: Record<string, string>;
  box?: { x: number; y: number; width: number; height: number };
  styles?: Record<string, string>;
  children?: LynxSnapshotNode[];
};
```

## Virtual Environment

Device profile is user-defined:

```ts
type DeviceProfile = {
  platform?: 'ios' | 'android' | 'desktop' | string;
  viewport: { width: number; height: number };
  dpr?: number;
  safeArea?: { top: number; right: number; bottom: number; left: number };
  orientation?: 'portrait' | 'landscape';
  locale?: string;
  timezone?: string;
  userAgent?: string;
  extra?: Record<string, unknown>;
};
```

v1 built-in behavior:

- Apply viewport and DPR to the headless render surface.
- Apply safe area and platform values through session/global configuration.
- Use touch input as the default mobile interaction path.
- Provide minimal lifecycle dispatch: foreground, background, resume, destroy.

No built-in standard mobile NativeModules library is provided in v1.

## NativeModules JS Mock

Mocks are JS-only and can be supplied as an object or module path.

```js
export default {
  device: {
    getInfo() {
      return { platform: 'ios', model: 'iPhone 15' };
    },
  },
  account: {
    async getUser() {
      return { id: 'test-user' };
    },
  },
};
```

Design requirements:

- Support sync functions.
- Support async functions.
- Support callback-style functions when Lynx code passes callbacks.
- Trace all mock calls with method name, arguments, return value/error, and
  timestamp.
- Inject through JS harness/preload/contextBridge where possible.
- Do not build JSON DSL or a standard mock registry.

## Artifacts

Default artifact set:

```text
artifacts/
  screenshot.png
  screenshot-after-tap.png
  ui-dump.json
  ui-dump-after-tap.json
  report.json
  trace.jsonl
  replay.json
```

`report.json` includes:

- runtime version
- package version
- platform/backend
- device profile
- bundle path/url/hash
- load status
- `ready-to-show` timing
- `on-first-screen` timing
- console errors
- Lynx errors
- bridge call summary
- NativeModules mock call summary
- screenshot paths
- trace path
- final result and exit code reason

`trace.jsonl` events include:

- runtime lifecycle
- protocol connection
- session create/destroy
- load start/success/error
- `ready-to-show`
- `on-first-screen`
- console messages
- exceptions
- Lynx errors
- bridge call/send/reply
- NativeModules mock call/return/error
- input actions
- screenshot events
- protocol method errors

## Error Model

All public SDK/protocol errors must be structured:

```ts
type HeadlessError = {
  ok: false;
  code: string;
  message: string;
  sessionId?: string;
  targetId?: string;
  nodeId?: number;
  details?: Record<string, unknown>;
  artifacts?: {
    report?: string;
    trace?: string;
    screenshot?: string;
  };
};
```

Initial error codes:

- `RUNTIME_LAUNCH_FAILED`
- `SESSION_CREATE_FAILED`
- `BUNDLE_LOAD_FAILED`
- `FIRST_SCREEN_TIMEOUT`
- `LYNX_RUNTIME_ERROR`
- `NODE_NOT_FOUND`
- `NODE_NOT_VISIBLE`
- `INPUT_DISPATCH_FAILED`
- `SCREENSHOT_FAILED`
- `NATIVE_MODULE_MOCK_FAILED`
- `PROTOCOL_METHOD_NOT_FOUND`
- `PROTOCOL_INTERNAL_ERROR`
- `CLI_CONFIG_ERROR`

CLI exit codes:

- `0`: success
- `1`: runtime/protocol failure
- `2`: load failure
- `3`: first-screen timeout
- `4`: Lynx runtime error
- `5`: CLI/config error

## Wait Strategy

Default smoke success:

```text
load request accepted
  -> ready-to-show
  -> on-first-screen
  -> optional stable frame wait
```

`waitForFirstScreen()` defaults:

```ts
{
  timeoutMs: 10000,
  waitForStableFrame: false,
  stableFrameCount: 2
}
```

The CLI waits for `on-first-screen` before taking the default screenshot. On
timeout it should still try to write `report.json`, `trace.jsonl`, and an error
screenshot if screenshot is available.

## Implementation Plan

### Phase 0: Design And Interfaces

- Add this design document.
- Define TypeScript public API declarations for SDK objects, errors, artifacts,
  device profile, locator query, and protocol messages.
- Define protocol domain/method names and payloads.

Acceptance:

- API draft compiles as TypeScript declarations.
- CLI command examples are represented in docs.
- No product scope depends on Playwright, MCP, or a test runner.

### Phase 1a: macOS True Headless Prototype

- Add headless runtime launch flag and harness entry.
- Add a single-session controller.
- Create LynxWindow/LynxView in headless mode.
- Decouple render active from visible window state in headless mode.
- Implement screenshot from the Lynx headless render surface.
- Route `Input.dispatchTouchEvent` through the existing DevTool event simulation
  proxy.
- Expose `ready-to-show`, `on-first-screen`, and Lynx error events through the
  protocol.
- Implement `report.json` and `trace.jsonl`.

Acceptance:

- On macOS, a CLI command can load a real `.lynx.bundle`.
- No user-visible window is required.
- The run reaches `on-first-screen`.
- A PNG screenshot is written.
- Trace records load, first screen, screenshot, and exit result.
- A tap action can trigger a Lynx event in a fixture page.

### Phase 1b: SDK And CLI

- Implement `@lynx-js/lynxtron-headless` SDK.
- Implement `lynxtron-headless run`.
- Implement JS NativeModules mock object/module path loading.
- Implement text/testId/basic-attribute locator.
- Implement structured errors and fixed exit codes.
- Expose `wsEndpoint()` for advanced users.

Acceptance:

- A Node script can launch runtime, load bundle, wait for first screen, tap a
  node, screenshot, and read report.
- CLI returns deterministic exit codes for success, load failure, timeout, and
  config error.
- JS mock calls are visible in `trace.jsonl`.

### Phase 1c: Linux CI/Cloud

- Add Linux true headless backend.
- Stabilize fonts, DPR, viewport, and screenshot output.
- Add CI smoke job for headless bundle load and screenshot.

Acceptance:

- Linux CI can run the CLI without a user-visible desktop.
- The same fixture bundle passes load, first screen, screenshot, and tap smoke.
- Artifacts are collected in CI.

## Validation Fixtures

Create minimal fixture bundles for:

- static first screen with text and test ids
- tap changes text
- NativeModules JS mock call
- bridge `call` and `send`
- load failure
- first-screen timeout

Each fixture should be small and deterministic.

## Risks And Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| True headless render surface is deeper than expected | v1 cannot be proven with only JS/CDP work | Make Phase 1a focus on runtime headless before SDK polish. |
| Current render active requires `IsVisible()` | Hidden windows may not render | Add explicit headless render active mode. |
| Screenshot path is not currently public | CLI cannot generate artifacts | Implement native screenshot from Lynx render surface, not WebContents. |
| DOM/CSS inspection APIs are not currently exposed as JS APIs | Locator/snapshot may slip | Implement gateway backed by Lynx/DevTool internals; keep v1 locator narrow. |
| Linux headless differs from macOS | CI value delayed | Keep backend abstraction from macOS prototype; make Linux acceptance a separate phase. |
| NativeModules mocks can become a second framework | Scope creep | JS-only mocks, no standard registry, no JSON DSL. |
| CDP compatibility expectations grow | Scope creep | Document stable method subset and custom Lynx domains. |

## Open Technical Unknowns

These need source-level investigation during Phase 1a:

- Exact native API to capture pixels from Lynx headless render surface.
- Exact Lynx/DevTool API for node tree, box model, text, and computed styles.
- Whether `SetEventSimulationProxy` is enough for all v1 touch/scroll paths.
- Best way to inject JS NativeModules mocks into BTS without conflicting with
  existing `nodejs` and `bridge` modules.
- Whether macOS headless can run without creating an NSWindow at all, or whether
  it needs a non-visible render host as an intermediate step.
- Linux compositor/software-render requirements for no-display CI.

## First Engineering Slice

The first executable slice should not start with SDK polish. It should prove the
runtime path:

1. Add a headless launch mode and harness entry.
2. Create a single LynxView session in headless mode.
3. Load a fixture `.lynx.bundle`.
4. Emit `ready-to-show`, `on-first-screen`, and error events to an internal trace.
5. Capture a screenshot.
6. Dispatch one tap through DevTool input simulation.
7. Exit with `report.json` and `trace.jsonl`.

Only after this slice works should the public SDK be filled out.

## Current Implementation Snapshot

Accepted macOS slice:

- Runtime entry: `src/packages/lynxtron-headless/harness/main.js`
- SDK and CLI: `src/packages/lynxtron-headless/`
- Native window API: `src/shell/api/api_lynx_window.cc`
- LynxView builder hook: `src/shell/api/lynx_view/lynx_view_builder.*`
- Windowless renderer: `src/shell/api/lynx_view/headless_windowless_renderer.*`
- Lynx same-loop task runner fix:
  `lynx/core/base/threading/task_runner_manufactor.cc`
- Smoke fixture:
  `src/packages/lynxtron-headless/fixtures/smoke-app/`

The macOS proof uses `new LynxWindow({ show: false, headless: true,
deviceScaleFactor })`. In this mode `LynxWindow` installs a
`HeadlessWindowlessRenderer`, keeps render active independent of
`NativeWindow::IsVisible()`, captures the latest software-presented frame as
PNG, and dispatches pointer events through the windowless renderer. This is the
accepted true-headless path; hidden native-window rendering is not accepted as
headless.

The SDK/CLI slice currently proves:

- load a real `.lynx.bundle`
- observe `ready-to-show`
- observe `on-first-screen`
- capture `screenshot.png`
- dispatch one tap
- verify the post-tap frame changed
- write `report.json` and `trace.jsonl`
- exit with deterministic status codes

The accepted smoke result is recorded in `product-plan.md` and `status-log.md`.

Current limitation:

- The software windowless path is accepted for the deterministic no-image
  fixture only. Image/texture-heavy bundles currently crash in the Clay image
  producer at `lynx/clay/gfx/image/image_producer.cc:566` with
  `unref_queue->GetContext()`. HRT-011 owns this follow-up.
