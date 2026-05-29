# Project Goal

## Summary

Build `@lynx-js/lynxtron-headless`: a true headless Lynxtron SDK/runtime for
running real Lynx bundles in CI, cloud development, and AI/tooling
environments.

The SDK and CLI drive a headless Lynxtron binary. The public API is Lynx-aware.
UI observation and operation are exposed through a CDP-compatible control plane
and Lynx custom protocol domains.

## Final Goal

The v1 product lets a user launch a headless Lynxtron runtime, create a
controlled bundle session, configure a mobile-like virtual environment, inject
JS NativeModules mocks, wait for first screen, inspect and operate the Lynx UI,
capture artifacts, and close the runtime with structured diagnostics.

The first proof target is macOS. The v1 product acceptance target includes
Linux CI/cloud support. Windows is out of scope for v1.

## Target Users

- Lynx application developers who need fast local smoke checks.
- CI owners who need deterministic Lynx bundle validation.
- Cloud development platforms that need remote Lynx previews and diagnostics.
- AI skill/tool authors who need UI observation and operation primitives.
- Future Playwright adapter and MCP tool authors.

## Core Scenarios

- Run a Lynx bundle in CI and fail with stable exit codes when load or first
  screen fails.
- Capture screenshot, UI dump, `report.json`, and `trace.jsonl` for a failed or
  successful run.
- Simulate a mobile device profile with viewport, DPR, safe area, locale,
  lifecycle, and touch input.
- Inject JS NativeModules mocks from a module path or object and trace calls.
- Let an external tool connect to the runtime WebSocket endpoint and operate
  the Lynx UI through a documented protocol subset.
- Allow an AI tool to inspect text, test ids, boxes, styles, errors, bridge
  calls, and NativeModules calls.

## Functional Description

The product contains:

- One npm package: `@lynx-js/lynxtron-headless`.
- A Node SDK for lifecycle, session, loading, locator, input, screenshot,
  artifacts, and diagnostics.
- A CLI binary: `lynxtron-headless`.
- A protocol client and public CDP-compatible WebSocket endpoint.
- A headless Lynxtron harness app and single-session controller.
- A true headless runtime surface that does not rely on a user-visible OS
  window.

## In Scope

- Harness-created bundle sessions.
- True headless runtime work, not only `show: false`.
- macOS prototype first.
- Linux CI/cloud backend after macOS proof.
- One active session per runtime process.
- JS-only NativeModules mocks.
- Customizable virtual device profiles.
- Lynx semantic locators: text, test id, basic attributes, and node ids.
- Stable artifacts: screenshot(s), UI dump(s), `report.json`, `trace.jsonl`.
- Structured SDK/protocol errors and fixed CLI exit codes.
- CDP-compatible UI operation and observation subset.
- Lynx custom protocol domains for runtime, device, lifecycle, mocks, bridge,
  and artifacts.

## Out Of Scope

- Playwright adapter implementation in v1.
- MCP server/tool implementation in v1.
- Test runner, assertion library, reporter UI, snapshot diff, or visual
  regression framework.
- Arbitrary attach to a user-provided full Lynxtron app in v1.
- Independent Node native addon in v1.
- Built-in iOS/Android standard NativeModules mock library.
- JSON mock DSL.
- Windows v1 support.

## Success Criteria

Phase 0 success:

- Product goal, product plan, workflow, status log, and engineering design are
  written under `docs/headless-runtime/`.
- Phase 1a first engineering task has a clear scope, file ownership, acceptance
  criteria, and verification method.

Phase 1a macOS proof success:

- A CLI command can load a real `.lynx.bundle` on macOS without a user-visible
  window.
- The run reaches `on-first-screen`.
- A PNG screenshot is written from the Lynx render surface.
- `report.json` and `trace.jsonl` are written.
- A tap action can trigger a Lynx event in a fixture page.
- Current accepted proof uses a no-image fixture and Lynx windowless software
  rendering. Image/texture-heavy bundles remain a follow-up.

Phase 1b SDK/CLI success:

- A Node script can launch runtime, load bundle, wait for first screen, tap a
  node, screenshot, and read report.
- Runtime-local CDP can load, screenshot, dump the Lynx UI tree, dispatch
  touch input, dump the UI tree again, and observe changed UI state.
- A run writes `replay.json`, and the CLI can replay it in semantic and exact
  modes.
- A record run observes provider input through `LynxRecorder.*` and can replay
  recorded input actions.
- A developer can run or replay the CDP flow with `--headed --slow-mo` for
  authoring/debug visibility.
- CLI returns deterministic exit codes for success, load failure, timeout, and
  config error.
- JS NativeModules mock calls are visible in `trace.jsonl`.

Current MVP success:

- The macOS no-image MVP is accepted for true headless runtime, SDK/CLI launch,
  runtime-local CDP endpoint, screenshot, native-backed UI dump, CDP touch
  input, after-dump assertion, replay recording, replay execution, and artifact
  collection.
- Headed authoring playback is accepted as a debug mode over the same
  windowless/CDP path.
- CDP recorder monitoring is accepted for the generic provider schema,
  macOS provider compile path, and recorded-action replay.
- The accepted complex smoke derives the tap target from `Lynx.dumpUITree` and
  does not assert by reading JS app state directly.

Phase 1c Linux success:

- Linux CI can run the CLI without a user-visible desktop.
- The same fixture bundle passes load, first screen, screenshot, and tap smoke.
- CI collects artifacts for success and failure cases.

## Open Questions

- Required native fix for image/texture-heavy bundles in the software
  windowless path.
- Computed style and full DOM/CSS inspector parity remain open. MVP node tree,
  text, and viewport-absolute boxes are covered by `Lynx.dumpUITree`.
- Whether the existing DevTool event simulation proxy is enough for all v1
  touch and scroll paths.
- Best JS injection point for NativeModules mocks without conflicting with
  existing `nodejs` and `bridge` modules.
- Whether macOS headless can avoid creating `NSWindow` entirely, or needs a
  non-visible render host as an intermediate step.
- Linux no-display requirements for compositor, software rendering, fonts, DPR,
  and screenshot determinism.
