# Product Plan

## Current Definition

`@lynx-js/lynxtron-headless` is a headless Lynxtron runtime package that exposes
a Node SDK and CLI for running and operating Lynx bundle sessions in
non-interactive environments. The API shape keeps CDP, Playwright-style
drivers, and AI tool users in mind, but those adapter layers are not part of
the current implementation slice.

The product must prove real runtime capability. A JS wrapper around existing
visible or hidden Lynxtron windows is not enough.

The source-backed design is in `docs/headless-runtime/engineering-design.md`.

## MVP Boundary

Status: Accepted on macOS for deterministic no-image Lynx bundles.

The current MVP is complete only for this scoped product slice:

- launch a true headless Lynxtron runtime on macOS
- load a real `.lynx.bundle`
- expose a runtime-local CDP-compatible WebSocket endpoint
- drive UI observation and operation through CDP, including
  `Page.takeScreenshot`, `Lynx.dumpUITree`, and `Input.dispatchTouchEvent`
- write screenshot, before/after UI dump, report, and trace artifacts
- prove interaction by taking a UI dump, deriving a tap target from that dump,
  sending CDP input, taking another UI dump, and asserting changed Lynx UI state
- record a `replay.json` manifest with source/runtime/device/load/action
  metadata and artifact hashes
- replay the recorded run in semantic mode and exact coordinate mode
- optionally run or replay in authoring headed mode with a visible host window
  and `slowMo` delays for human debugging
- expose a generic CDP recorder domain for observed input events, with a
  macOS Lynxtron provider and recorded-action replay mode

This MVP does not include Linux CI/cloud, image/texture-heavy bundles, JS
NativeModules mock loading, Playwright adapter, MCP tool/server, visual diff, or
full Chrome CDP compatibility. Mobile input providers are a follow-up; the
current recorder provider is macOS Lynxtron plus generic CDP provider injection.

## Milestones

### Phase 0: PM And Interface Freeze

Status: Done.

Goal:

- Freeze product goal, plan, workflow, and first task card.
- Keep all decisions source-backed and reviewable.

Acceptance:

- PM document set exists under `docs/headless-runtime/`.
- First engineering task can be handed to a worker without additional product
  clarification.

### Phase 1a: macOS True Headless Prototype

Status: Accepted for macOS no-image fixtures.

Goal:

- Prove that Lynx can load, render, screenshot, and receive input in headless
  mode on macOS.

Acceptance:

- Runtime launches in explicit headless mode.
- No user-visible window is required.
- A fixture bundle reaches `on-first-screen`.
- Screenshot, report, and trace artifacts are written.
- A tap action changes fixture state.
- The native backend is Lynx windowless software rendering, not a hidden
  user-visible window.

### Phase 1b: SDK, CLI, And CDP Control Plane

Status: MVP accepted; broader v1 remains partially accepted.

Goal:

- Wrap the Phase 1a runtime path in the public npm package, SDK, CLI, runtime
  CDP endpoint, protocol client, JS mocks, locators, structured errors, and exit
  codes.

Acceptance:

- SDK script and CLI command can run the smoke path.
- Runtime-local CDP endpoint can load, wait, screenshot, dump UI, dispatch
  touch input, and observe the updated UI.
- CLI can replay a recorded `replay.json` manifest in semantic and exact modes.
- CLI can run/replay in headed authoring mode with slow-motion CDP playback.
- CLI can record observed input through `LynxRecorder.*` and replay recorded
  input actions from `replay.json`.
- CLI exit codes are deterministic for success, load failure, timeout, config
  error, and input no-change failure.
- NativeModules JS mock calls and public locator APIs remain follow-up work.

### Phase 1c: Linux CI/Cloud

Status: Not started.

Goal:

- Port the true headless backend to Linux CI/cloud and make artifacts
  deterministic enough for smoke validation.

Acceptance:

- Linux CI runs without a user-visible desktop session.
- The same fixture bundle passes load, first-screen, screenshot, and tap smoke.
- Artifacts are collected in CI.

## Prioritized Tasks

| ID | Priority | Status | Task | Primary Output |
| --- | --- | --- | --- | --- |
| HRT-000 | P0 | Done | Create source-backed engineering design | `engineering-design.md` |
| HRT-001 | P0 | Done | Source-level feasibility map for Phase 1a | `spikes/HRT-001-phase-1a-source-map.md` |
| HRT-002 | P0 | Done | Add explicit macOS headless runtime launch mode | Runtime flag and harness entry |
| HRT-003 | P0 | Done | Decouple headless render active state from window visibility | Native runtime patch |
| HRT-004 | P0 | Done | Implement minimal first-screen trace and structured errors | Trace/report skeleton |
| HRT-005 | P0 | Done | Capture screenshot from Lynx render surface | PNG artifact |
| HRT-006 | P0 | Done | Route one tap through windowless pointer input | Tap smoke fixture |
| HRT-007 | P1 | Initial done | Draft public TypeScript API declarations | SDK type draft |
| HRT-008 | P1 | Done | Implement CLI smoke command | `lynxtron-headless run` |
| HRT-009 | P1 | Pending | Implement JS NativeModules mock loading and trace | Mock runtime support |
| HRT-010 | P2 | Pending | Linux headless backend and CI smoke | Linux validation |
| HRT-011 | P0 | Pending | Fix image/texture path for software windowless bundles | Default app smoke support |
| HRT-012 | P0 | Done | Add runtime-local CDP endpoint and protocol client | CDP control plane |
| HRT-013 | P0 | Done | Back `Lynx.dumpUITree` from native windowless UI tree | CDP UI observation |
| HRT-014 | P0 | Done | Add complex CDP smoke with screenshot, dump, input, after-dump assert | MVP acceptance smoke |
| HRT-015 | P0 | Done | Add replay manifest recording and semantic/exact replay | Reproducible smoke |
| HRT-016 | P1 | Done | Add headed authoring playback with slowMo | Visual debug run/replay |
| HRT-017 | P0 | Done | Add generic CDP input monitor and recorded replay mode | Recorder CDP domain |

## Dependencies And Risks

| Dependency Or Risk | Impact | Current Plan |
| --- | --- | --- |
| Headless render surface may require deeper native work | Phase 1a can slip | Start with source-level feasibility map before SDK polish. |
| Current render active path depends on visibility | Hidden windows may not render | Add explicit headless render active mode. |
| Screenshot path is not public today | CLI artifacts blocked | Implement capture from Lynx render surface, not WebContents. |
| Image/texture upload path crashes in current software windowless proof | Bundles with real images such as `default_app` cannot be accepted yet | Add HRT-011; current acceptance uses a deterministic no-image fixture. |
| DOM/CSS inspection APIs may not be exposed at JS layer | Locator/snapshot scope can slip | Keep v1 locator narrow and use Lynx/DevTool internals. |
| NativeModules mocks can expand into a framework | Scope creep | JS-only object/module mocks, no registry, no JSON DSL. |
| CDP compatibility expectations can grow | Scope creep | Ship only the MVP subset and use Lynx custom domains for Lynx-specific state. |
| Headed authoring uses the windowless renderer control path | It may not represent the final true native GUI renderer model | Treat `--headed` as debug/authoring mode only; CI acceptance remains true headless. |
| Real desktop input recording depends on platform event providers | macOS and mobile capture paths can diverge | Keep public recording as `LynxRecorder.*` CDP events; platform code only supplies provider events. |
| Linux behavior can differ from macOS | CI value delayed | Treat Linux as a separate milestone after macOS proof. |

## Decisions

- Use the existing Lynxtron runtime/binary path rather than an independent Node
  native addon.
- v1 supports harness-created bundle sessions only.
- UI observation and operation must go through the runtime-local
  CDP-compatible WebSocket endpoint. Native APIs may back the endpoint but are
  not the durable public control plane.
- Do not ship Playwright adapter or MCP tool in v1.
- Keep the SDK operation/observation focused and exclude assertions, runner,
  reporter, and visual diff.
- Use `on-first-screen` as the default success and screenshot wait signal.
- Use one active session per runtime process.
- Use JS-only NativeModules mocks and never introduce a JSON mock DSL.
- Ship one npm package first: `@lynx-js/lynxtron-headless`.
- The macOS proof uses `LynxWindow({ headless: true })` with Lynx
  `LynxWindowlessRenderer` software rendering. Hidden native-window rendering
  does not satisfy the product requirement.
- Phase 1a acceptance is scoped to the checked-in no-image smoke fixture until
  HRT-011 resolves the image/texture crash.
- The MVP CDP smoke uses a checked-in complex no-image fixture and asserts
  state changes from CDP UI dumps rather than direct JS state access.
- Input recording is represented as CDP recorder events/actions. macOS
  `NSWindow` input capture is only one provider; mobile providers should emit
  the same schema.

## Change Log

| Date | Change |
| --- | --- |
| 2026-05-28 | Created source-backed engineering design. |
| 2026-05-28 | Created PM document set and first Phase 1a task card. |
| 2026-05-28 | Completed HRT-001 source-level feasibility map. |
| 2026-05-28 | Implemented and accepted macOS true headless smoke for the no-image fixture. |
| 2026-05-28 | Added initial `@lynx-js/lynxtron-headless` SDK/CLI and recorded HRT-011 image-path follow-up. |
| 2026-05-28 | Accepted the macOS MVP CDP slice with screenshot, native-backed UI dump, CDP input, after-dump assert, and complex fixture smoke. |
| 2026-05-28 | Added `replay.json` recording and accepted semantic/exact replay from a recorded complex smoke. |
| 2026-05-28 | Added headed authoring playback with `--headed` and `--slow-mo` for run/replay debugging. |
| 2026-05-28 | Added `LynxRecorder.*` CDP input monitoring, macOS provider bridge, `record` command, and recorded replay mode. |
| 2026-05-29 | Added the MVP evaluation summary covering project starting point, target, measured results, and backend extension space. |

## Current Status

The scoped macOS MVP is in a verifiable accepted state for checked-in no-image
fixtures.

Primary MVP acceptance command:

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/complex-app/dist/headless_complex.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-complex \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --smoke complex
```

Primary accepted artifacts:

- `/tmp/lynxtron-headless-complex/report.json`
- `/tmp/lynxtron-headless-complex/trace.jsonl`
- `/tmp/lynxtron-headless-complex/screenshot.png`
- `/tmp/lynxtron-headless-complex/screenshot-after-tap.png`
- `/tmp/lynxtron-headless-complex/ui-dump.json`
- `/tmp/lynxtron-headless-complex/ui-dump-after-tap.json`
- `/tmp/lynxtron-headless-complex/replay.json`

Primary observed result:

- `status: success`
- `exitCode: 0`
- `backend: windowless-software`
- `protocol.cdp.wsEndpoint` present
- `headless.headless: true`
- `headless.hasRenderer: true`
- `input.tap.source: ui-dump`
- `input.tap.protocol: cdp`
- before UI dump contains `Status idle`, `Selected Basic`, `Cart total $42`,
  and `Items 3`
- after UI dump contains `Status selected`, `Selected Pro`, `Cart total $57`,
  and `Items 4`
- screenshot changed after CDP input
- `replay.json` records bundle sha256, runtime metadata, device profile, load
  options, CDP action params, semantic tap text, and artifact hashes

Replay acceptance commands:

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-replay-semantic
```

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-replay-exact \
  --mode exact
```

Replay observed result:

- both commands exit with code `0`
- both reports have `status: success`
- semantic replay has `input.tap.source: ui-dump`
- exact replay has `input.tap.source: cli`
- both replays preserve the expected before/after UI dump text assertions

Headed authoring acceptance commands:

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-headed-replay \
  --headed \
  --slow-mo 50
```

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/complex-app/dist/headless_complex.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-headed-run \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --smoke complex \
  --headed \
  --slow-mo 10
```

Headed authoring observed result:

- both commands exit with code `0`
- reports include `authoring.headed: true`
- reports include `authoring.rendererMode: windowless-renderer-visible-host`
- traces include `authoring.headed-window`
- traces include `cdp.slow-mo` events
- replay/run still preserve the expected CDP UI dump behavior assertions

CDP recorder acceptance commands:

```sh
node src/packages/lynxtron-headless/cli.js record \
  src/packages/lynxtron-headless/fixtures/complex-app/dist/headless_complex.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-record-injected \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --smoke complex \
  --record-duration 4000 \
  --cdp-port 19937
```

During this command, an external CDP client sent two
`LynxRecorder.dispatchObservedInputEvent` provider events at `195,611`.

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record-injected/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-recorded-replay \
  --timeout 12000
```

CDP recorder observed result:

- record exits with code `0`
- record report has `completionSignal: recorded-input`
- record report has `input.recording.eventCount: 2`
- record report has `input.recording.snapshotCount: 1`
- record report has `input.recording.changed: true`
- replay manifest has `replay.defaultMode: recorded`
- recorded replay exits with code `0`
- recorded replay report has `completionSignal: recorded-replay`
- after replay dump contains `Selected Pro`, `Cart total $57`,
  `Status selected`, and `Items 4`

Baseline smoke command:

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/smoke-app/dist/headless_smoke.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-acceptance \
  --timeout 10000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --tap 195,422
```

Baseline accepted artifacts:

- `/tmp/lynxtron-headless-acceptance/report.json`
- `/tmp/lynxtron-headless-acceptance/trace.jsonl`
- `/tmp/lynxtron-headless-acceptance/screenshot.png`
- `/tmp/lynxtron-headless-acceptance/screenshot-after-tap.png`
- `/tmp/lynxtron-headless-acceptance/ui-dump.json`
- `/tmp/lynxtron-headless-acceptance/ui-dump-after-tap.json`
- `/tmp/lynxtron-headless-acceptance/replay.json`

Baseline observed result:

- `status: success`
- `exitCode: 0`
- `backend: windowless-software`
- `completionSignal: on-first-screen`
- `headless: true`
- `hasRenderer: true`
- `input.tap.changed: true`
- before UI dump contains `tap 0`
- after UI dump contains `tap 1`

## Next Step

Next engineering slice should prioritize either HRT-011 image/texture support
for broader bundle coverage, HRT-009 JS mock loading for richer no-image test
bundles, or a mobile provider that emits the accepted `LynxRecorder` input
event schema.
