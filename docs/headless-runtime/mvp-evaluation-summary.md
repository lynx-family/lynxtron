# Lynxtron Headless Harness MVP Evaluation Summary

Date: 2026-05-29

## Project Starting Point

Lynx applications need a runtime-level way to run, inspect, and operate real
Lynx bundles outside a manually operated desktop app. The target scenarios are
CI smoke testing, cloud development, agent/AI tool execution, and future mobile
device automation.

The project deliberately avoids making the first product a thin JavaScript
wrapper around a visible Lynxtron app. The MVP must prove that Lynxtron can
provide a controlled runtime surface with real Lynx rendering, CDP-backed UI
observation, CDP-backed input, and stable machine-readable artifacts.

## Product Goal

Build `@lynx-js/lynxtron-headless`: a Lynx-aware SDK/runtime and CLI that can
launch a controlled Lynx runtime, load real `.lynx.bundle` files, observe and
operate UI through a CDP-compatible control plane, and produce deterministic
artifacts for validation and reproduction.

The public product shape is:

- SDK and CLI for lifecycle, loading, input, screenshot, UI dump, artifacts,
  diagnostics, and replay.
- CDP-compatible protocol as the durable observation and operation contract.
- Lynx custom protocol domains for runtime-specific state such as UI dump,
  recorder input, artifacts, and future mocks.
- One runtime session per process, with parallelism handled by launching more
  processes.
- True headless first for CI/cloud/agent usage, with headed authoring mode as a
  visual debugging path.

## MVP Scope

The accepted MVP is the macOS no-image Lynx bundle slice.

Included:

- true headless Lynxtron runtime using Lynx windowless software rendering
- runtime-local CDP-compatible WebSocket endpoint
- `Page.takeScreenshot`
- `Lynx.dumpUITree` backed by native/windowless UI data
- `Input.dispatchTouchEvent` for UI operation
- screenshot, before/after UI dump, `report.json`, `trace.jsonl`, and
  `replay.json`
- semantic replay and exact coordinate replay
- headed authoring playback over the same windowless/CDP path
- generic CDP recorder events with a macOS Lynxtron input provider
- complex checked-in smoke fixtures and assertion scripts
- a small harness skill that can be used by an agent with no project-specific
  background

Excluded from the MVP:

- Linux CI/cloud backend
- image/texture-heavy bundle acceptance
- JS NativeModules mock loading
- Playwright adapter
- MCP server/tool
- visual diff or test runner
- arbitrary attach to a user-provided Lynxtron app
- mobile device backend
- Windows support

## MVP Architecture

```text
SDK / CLI / future tool adapter
        |
        | launch process + CDP-compatible WebSocket
        v
Headless Lynxtron harness runtime
        |
        | load bundle, operate UI, collect artifacts
        v
LynxWindow(headless: true)
        |
        | windowless software renderer
        v
Lynx UI tree, frame capture, pointer input, trace/replay
```

Important design decisions:

- CDP is the observation and operation contract. Native APIs may back CDP
  methods, but the user-facing harness should not depend on private native
  calls.
- The harness is not a test framework. It provides runtime execution and
  artifacts; assertions can live in tests, CI scripts, Playwright adapters, MCP
  tools, or agent skills.
- The same contract should support future backends: macOS windowless, Linux
  windowless, Android ADB, iOS Simulator, and device cloud.

## Evaluation Results

### Core Harness Smoke

The complex fixture proves the core chain:

- load a real Lynx bundle
- wait for first screen
- take a screenshot
- dump the UI tree
- derive a tap target from the UI dump
- dispatch CDP input
- take another screenshot and UI dump
- assert changed UI state from artifacts
- write replay data
- replay the recorded run

Primary accepted artifact shape:

- `report.json`
- `trace.jsonl`
- `screenshot.png`
- `screenshot-after-tap.png`
- `ui-dump.json`
- `ui-dump-after-tap.json`
- `replay.json`

The accepted run reports:

- `backend: windowless-software`
- `headless.headless: true`
- `headless.hasRenderer: true`
- `input.tap.protocol: cdp`
- tap target source from UI dump
- changed UI state after input

### Agent Skill Experiment

HRT-018 tested whether a no-background agent could use a small harness skill to
build and validate a meaningful Lynx page.

Result:

- The worker built a complex Lynx fixture.
- The worker used the harness CLI to run the page headlessly.
- The worker produced screenshot, UI dump, after-tap UI dump, report, trace,
  replay, and assertion evidence.
- PM re-ran the build, headless run, assertion, replay, and replay assertion.

Conclusion:

The harness skill is sufficient to convert a natural-language page requirement
into executable Lynx runtime evidence. This is the core agent-tool value.

### A/B Commerce Experiment

HRT-019 compared two agents implementing the same ecommerce Lynx page:

- Agent A used the headless harness skill.
- Agent B used the Lynx DevTool skill and the shell-demo app.

First-round PM verification:

| Path | PM-verifiable time | Result |
| --- | --- | --- |
| Agent A: harness | 2026-05-29 10:06:57 | Passed true-headless product and profile smoke scenarios with artifact-backed assertions. Gap: home list did not initially prove real scrollability. |
| Agent B: shell/devtool | 2026-05-29 10:09:52 | Product states were reachable through DevTool after PM assembled a temporary shell app. Gap: screenshots showed overflow/viewport issues and the agent did not return a final delivery note. |

Fix iteration:

| Path | Result | Efficiency |
| --- | --- | --- |
| Agent A: harness fix | Accepted. Replaced the home list with `scroll-view` and added a `home-scroll` UI dump assertion. | Best. The agent returned a complete delivery note and artifacts; PM re-ran build, detail smoke, profile smoke, and scroll assertion. |
| Agent B: shell visual fix | Partially accepted for visual outcome. The UI became inspectable and screenshots improved. | Weaker. The agent did not autonomously close the loop; PM had to inspect, launch, and verify with DevTool. |

Observed product signal:

- Harness path gives repeatable verification commands and durable artifacts.
- Shell/devtool path is useful for visual inspection, but it needs a
  standardized build, launch, viewport, and screenshot harness before it can
  match the delegation efficiency of the headless path.

## Value Judgment

The MVP has product value because it turns Lynx runtime validation into a
protocol and artifact contract.

For CI:

- Run a real Lynx bundle without a visible desktop UI.
- Fail with structured reports and stable artifacts.
- Keep smoke checks independent from human inspection.

For cloud development:

- Run bundle previews and diagnostics in managed environments.
- Collect screenshots, UI trees, logs, traces, and replay manifests.

For AI skills and tools:

- Give agents a small, concrete execution surface.
- Let agents prove work by commands, reports, UI dumps, screenshots, and replay
  instead of plain text claims.
- Keep future Playwright and MCP adapters as consumers rather than forcing them
  into the core runtime.

For Lynxtron itself:

- Establishes a reusable headless/windowless runtime path.
- Keeps UI observation and operation aligned with CDP.
- Separates runtime capability from test-framework concerns.

## Known Gaps

- Current accepted runtime is macOS no-image only.
- Image/texture-heavy bundles still need a native follow-up.
- Linux CI/cloud backend is not implemented yet.
- JS NativeModules mock loading is not implemented yet.
- Scroll can be structurally asserted through `scroll-view`; stronger
  scroll-action validation should be added.
- Tap/frame-change detection had one observed `INPUT_NO_VISUAL_CHANGE` retry
  case during independent PM verification and needs more stability hardening.
- Shell/devtool visual validation is still manual unless a standardized
  headful launch and screenshot harness is added.

## Extension Space

### Linux CI/Cloud Backend

The next runtime backend should make the same CLI and SDK run in Linux
CI/cloud without a user-visible desktop session.

Expected contract:

- same bundle input
- same CDP endpoint
- same screenshot and UI dump artifacts
- same report, trace, and replay files
- same assertion scripts

### Mobile Backends

Mobile should be added as another backend/provider, not as a separate product.

Android shape:

```text
backend: android-adb
  -> install or start a thin Lynx explorer app
  -> push bundle or open a bundle URL/deeplink
  -> connect or forward the mobile Lynx DevTool/CDP endpoint
  -> use CDP-compatible screenshot, UI dump, input, trace, and replay
```

The explorer app should be minimal:

- host a LynxView
- accept bundle path, URL, schema, and device/mock options
- expose a stable DevTool/CDP endpoint
- report ready, crash, timeout, and lifecycle state
- route mocks through JS injection rather than a JSON mock DSL

iOS Simulator shape:

```text
backend: ios-sim
  -> boot/install/launch explorer app
  -> open bundle or URL
  -> connect DevTool/CDP
  -> collect screenshot, UI dump, logs, trace, replay
```

Device cloud shape:

```text
backend: remote-device-cloud
  -> allocate device
  -> install/start explorer app
  -> tunnel CDP endpoint
  -> collect artifacts from the harness contract
```

The key requirement is that mobile Lynx can expose stable CDP-compatible
observation and operation. If that holds, the upper SDK/CLI contract does not
need to change.

### Adapter Layer

Playwright adapter and MCP tool should remain downstream consumers until the
runtime contract is stable.

Likely adapter inputs:

- `wsEndpoint`
- UI dump node ids, text, boxes, and attributes
- screenshot
- input methods
- trace/replay files
- structured errors

## Recommended Next Steps

1. Stabilize the current macOS MVP and keep its smoke fixtures checked in.
2. Add stronger scroll-action validation to the harness.
3. Fix image/texture-heavy bundle rendering in the windowless software path.
4. Implement JS NativeModules mock loading and trace.
5. Standardize a headful launch/screenshot helper for visual review.
6. Port the backend to Linux CI/cloud.
7. Prototype Android ADB + explorer app as the first mobile backend.

