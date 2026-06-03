# Status Log

## 2026-06-03

### HRT-025 Open Interface Bug-Hunt Baseline

Active task:

- Expand open-interface E2E complexity until it finds real PC rendering,
  interaction, or JavaScript API defects.
- If the missing part is a reusable harness capability, add it through CDP or a
  Lynx custom CDP domain.

Progress:

- Strengthened `src/packages/lynxtron-interface-e2e` from a simple interaction
  smoke into a five-step CDP interaction chain:
  1. `Run interaction smoke`
  2. `Nested child action`
  3. `Catch child action`
  4. `Scroll row 4`
  5. `Prime form state`
- Added nested `catchtap` coverage:
  - final child state: `catch child seen`
  - final parent state: `catch parent idle`
- Kept scroll-view rendering and initial-state coverage in the green baseline.
- Tried adding `Scroll row 4` to the same sequence and found a current
  scroll-view clipping gap:
  - target came from UI dump
  - target was outside its nearest `scroll-view` clipping box
  - direct tap produced no visual/UI change
- Added the custom CDP method `LynxInput.scrollIntoView`.
- Updated semantic `--tap-text` execution to detect scroll ancestor clipping,
  scroll the container through CDP touch move events, re-dump UI, and then tap
  the updated target.
- HRT-020-G06 is now initially fixed for scroll-view child clipping on macOS
  Lynxtron; broader wheel/drag/page-scroll/mobile behavior remains follow-up.
- Added task card:
  `docs/headless-runtime/tasks/HRT-025-open-interface-bug-hunt.md`.

Verification:

- `node --check src/packages/lynxtron-interface-e2e/scripts/run-smoke.mjs`:
  passed.
- `node --check src/packages/lynxtron-interface-e2e/scripts/assert-ui.mjs`:
  passed.
- `npm --prefix src/packages/lynxtron-headless test`: passed, 5 tests.
- `npm --prefix src/packages/lynxtron-interface-e2e run smoke`: passed.

Smoke evidence:

- Headless runtime backend: `windowless-software`.
- Bundle: `dist/lynx_open_interface_e2e.bundle`, 107.7 kB.
- Screenshot: 213600 bytes.
- After-tap screenshot: 218784 bytes.
- Trace: 18849 bytes.
- `ui-snapshot.json`: 55357 bytes.
- `ui-snapshot-after-tap.json`: 55390 bytes.
- Report recorded a five-step CDP tap sequence with every tap changed.
- Step 4, `Scroll row 4`, recorded `scrolled: true` and `scrollCount: 1`.

### HRT-025 Event Propagation Finding

Active task:

- Continue expanding E2E coverage until a real Lynx open-interface behavior
  mismatch is found.

Progress:

- Added an `Interaction bug hunt lab` to
  `src/packages/lynxtron-interface-e2e/src/app.tsx`.
- Added a seven-step CDP semantic tap sequence:
  1. `Run interaction smoke`
  2. `Event matrix inner action`
  3. `Nested child action`
  4. `Catch child action`
  5. `Scroll row 4`
  6. `Reorder keyed items`
  7. `Prime form state`
- Updated harness change detection so a tap is accepted when either screenshot
  pixels or the CDP UI dump changes. This fixed false negatives for offscreen
  state changes.
- The latest bug-hunt smoke intentionally fails only on the event propagation
  assertion:
  - expected:
    `event matrix result outer-capture>inner-bind>middle-bind>outer-bind`
  - actual:
    `event matrix result inner-bind>middle-bind>outer-bind`

Evidence:

- `npm --prefix src/packages/lynxtron-interface-e2e run smoke`: fails in
  `scripts/assert-ui.mjs` with the missing `outer-capture` text.
- `report.json` records all seven CDP taps with `changed: true`.
- The same run passes nested `bindtap`, nested `catchtap`, scroll-view clipped
  row interaction, keyed reorder, form state, animation final state, screenshot,
  UI dump, and normalized UI snapshot checks.
- `npm --prefix src/packages/lynxtron-headless test`: passed, 5 tests.

Finding record:

- `docs/headless-runtime/experiments/HRT-025-engine-findings.md`

Current interpretation:

- This is an open-interface/runtime mismatch candidate: `capture-bindtap` is
  declared in `lynx/js_libraries/types/types/common/events.d.ts`, but the PC
  headless runtime path did not invoke the capture handler in this fixture.

### HRT-025 Nested Scroll And Text Selection Conflict

Active task:

- Extend bug-hunt E2E from tap-only interactions to real drag gestures over
  nested scroll and selectable text areas.

Progress:

- Added semantic drag support to the harness:
  - CLI: `--drag-text <text>:<dx>,<dy>`
  - SDK option: `dragText` / `dragTexts`
  - Report field: `input.tap.dragSequence`
- Added `Nested scroll and selection conflict` fixture coverage:
  - outer `scroll-view` with `bindscroll`
  - inner `scroll-view` with `bindscroll`
  - selectable `text` with `text-selection`, `custom-text-selection`,
    `bindselectionchange`, and `bindtap`

Verification:

- `node --check` for harness, CLI, SDK, and E2E scripts: passed.
- `npm --prefix src/packages/lynxtron-headless test`: passed, 5 tests.
- `npm --prefix src/packages/lynxtron-interface-e2e run build`: passed,
  bundle size 122.6 kB.

Control result:

- Command with only `--drag-text 'Nested scroll row 4:0,-120'` after the
  seven tap setup completed successfully.
- Report recorded one drag with `changed: true`.
- Final visible text reported:
  `nested scroll outer 0 inner 21`.
- This suggests the inner scroll route can receive the drag without routing the
  gesture to the outer scroll container.

Crash result:

- Command with only
  `--drag-text 'Selectable conflict text drag target:0,-80'` after the same
  seven tap setup crashed the Lynxtron process.
- Fatal log:
  `Check failed: !delta.IsOrigin(). delta should not be empty`.
- Key stack frames:
  `clay::ScrollView::DidScroll`,
  `clay::NestedScrollable::DoScroll`,
  `clay::NestedScrollManager::DispatchScroll`, and
  `clay::NestedScrollManager::OnDynamicAnimationUpdate`.

Finding record:

- `docs/headless-runtime/experiments/HRT-025-engine-findings.md`, F02.

Current interpretation:

- This is a runtime crash bug candidate. A drag over selectable text in a
  nested scroll context should not abort the process, even if text selection is
  unsupported or partially supported in the PC runtime path.

### HRT-023 CDP UI Snapshot Initial Implementation

Active task:

- Add a protocol-facing normalized UI snapshot so tests and agents can consume
  a screenshot-correlatable page shape without depending on unstable raw DOM
  paths.

Progress:

- Added `LynxSnapshot.capture` to the harness CDP method table.
- The method derives a normalized snapshot from the existing CDP-backed
  `Lynx.dumpUITree` data.
- Added default artifacts:
  - `ui-snapshot.json`
  - `ui-snapshot-after-tap.json`
- Updated CLI/SDK artifact mapping and TypeScript declarations.
- Updated HRT-020 assertions to consume snapshot data:
  - viewport-visible text through `visibleTextRuns`
  - full-page text through `documentTextRuns`
  - action candidate label and positive box
  - visual health non-blank check
  - protocol contract check for `protocol: cdp` and
    `method: LynxSnapshot.capture`

Verification:

- `node --check src/packages/lynxtron-headless/harness/main.js`: passed.
- `node --check src/packages/lynxtron-headless/lib/index.js`: passed.
- `node --check src/packages/lynxtron-headless/cli.js`: passed.
- `node --check src/packages/lynxtron-interface-e2e/scripts/assert-ui.mjs`:
  passed.
- `npm --prefix src/packages/lynxtron-headless test`: passed, 5 tests.
- `npm --prefix src/packages/lynxtron-interface-e2e run smoke`: passed.

Smoke evidence:

- Runtime backend: `windowless-software`.
- `ui-snapshot.json`: emitted and non-empty.
- `ui-snapshot-after-tap.json`: emitted and non-empty.
- HRT-020 snapshot assertion found `Run interaction smoke` as an action
  candidate with a positive box.
- HRT-020 after snapshot contained `tap result active` in visible text and
  `motion final done` in document text.

Decision:

- HRT-023 is accepted as an initial macOS Lynxtron implementation.
- The public product boundary remains CDP/custom CDP. Mobile and non-Lynxtron
  hosts should implement provider bridges behind the same
  `LynxSnapshot.capture` method.

Remaining limits:

- Mobile explorer provider is not implemented.
- Non-Lynxtron bridge provider is not implemented.
- Tiny action target and richer visual health diagnostics remain follow-up
  refinements.

### HRT-020 Multi-Step Interaction Supplement

Active task:

- Check whether the open-interface E2E interaction case can be strengthened
  after adding CDP snapshot support.

Progress:

- Added repeated `--tap-text` support to the harness and CLI path.
- Added SDK `tapTexts` argument mapping and TypeScript declaration.
- Improved text-target tap selection so smaller visible buttons, such as
  `Prime form state`, are selected instead of an oversized ancestor container.
- Updated HRT-020 smoke to execute three CDP taps:
  1. `Nested child action`
  2. `Prime form state`
  3. `Run interaction smoke`
- Updated assertions to require:
  - final form state text: `form programmatic state alpha-42 line two`
  - final tap/animation text
  - `report.input.tap.sequence.length === 3`
  - each tap has `protocol: cdp` and `changed: true`

Verification:

- `node --check src/packages/lynxtron-headless/harness/main.js`: passed.
- `node --check src/packages/lynxtron-headless/lib/index.js`: passed.
- `node --check src/packages/lynxtron-headless/cli.js`: passed.
- `node --check src/packages/lynxtron-interface-e2e/scripts/assert-ui.mjs`:
  passed.
- `npm --prefix src/packages/lynxtron-headless test`: passed, 5 tests.
- `npm --prefix src/packages/lynxtron-interface-e2e run smoke`: passed.

Smoke evidence:

```json
[
  {
    "index": 1,
    "targetText": "Prime form state",
    "source": "ui-dump",
    "changed": true,
    "protocol": "cdp",
    "framesBefore": 2,
    "framesAfter": 3
  },
  {
    "index": 2,
    "targetText": "Run interaction smoke",
    "source": "ui-dump",
    "changed": true,
    "protocol": "cdp",
    "framesBefore": 3,
    "framesAfter": 5
  }
]
```

Remaining gap:

- `Mark scroll row` is offscreen at y=956 for the current 844px viewport.
  This intermediate gap was superseded by the later HRT-025 implementation of
  `LynxInput.scrollIntoView` for scroll-view child clipping. Generic page
  scroll, wheel input, drag API shape, and mobile-provider coverage remain
  follow-up work.

### HRT-020 Nested Interaction Supplement

Active task:

- Verify nested interaction expectations for components with nested event
  surfaces.

Progress:

- Added `Nested event surface` to the HRT-020 fixture.
- The fixture renders a parent `view` with `bindtap` and a nested child `view`
  with its own `bindtap`.
- Updated the smoke interaction sequence to:
  1. `Nested child action`
  2. `Prime form state`
  3. `Run interaction smoke`
- Updated assertions to require both nested states after the child tap:
  - `nested parent seen`
  - `nested child seen`

Verification:

- `node --check src/packages/lynxtron-interface-e2e/scripts/run-smoke.mjs`:
  passed.
- `node --check src/packages/lynxtron-interface-e2e/scripts/assert-ui.mjs`:
  passed.
- `npm --prefix src/packages/lynxtron-interface-e2e run smoke`: passed.

Smoke evidence:

```json
[
  {
    "index": 1,
    "targetText": "Nested child action",
    "changed": true,
    "protocol": "cdp",
    "framesBefore": 2,
    "framesAfter": 3
  },
  {
    "index": 2,
    "targetText": "Prime form state",
    "changed": true,
    "protocol": "cdp",
    "framesBefore": 3,
    "framesAfter": 4
  },
  {
    "index": 3,
    "targetText": "Run interaction smoke",
    "changed": true,
    "protocol": "cdp",
    "framesBefore": 4,
    "framesAfter": 6
  }
]
```

Final UI evidence:

- `nested parent seen`
- `nested child seen`
- `form programmatic state alpha-42 line two`
- `tap result active`

## 2026-05-28

### Active Task

Phase 0 PM kickoff for Lynxtron headless runtime.

### Progress

- Created source-backed engineering design in
  `docs/headless-runtime/engineering-design.md`.
- Created PM document set under `docs/headless-runtime/`.
- Defined first delegable Phase 1a task card:
  `docs/headless-runtime/tasks/HRT-001-phase-1a-source-map.md`.
- Completed HRT-001 source-level feasibility map:
  `docs/headless-runtime/spikes/HRT-001-phase-1a-source-map.md`.

### Verification

- Verified expected PM files exist under `docs/headless-runtime/`.
- Verified HRT-001 source map exists under `docs/headless-runtime/spikes/`.
- Source map records the `rg` and `sed` commands used for source validation.

### Blockers

No product blocker for the PM document set.

Implementation risk recorded: screenshot capture from the Lynx render surface
does not yet have a confirmed existing API.

### Next Action

Review HRT-001 and begin HRT-002 as the first product-code engineering slice.

### Phase 1a/1b Acceptance Update

Active task:

- Implement the SDK/runtime to the point where the user can validate the PM
  acceptance path locally.

Progress:

- Added a macOS headless LynxWindow path backed by Lynx windowless software
  rendering.
- Added native headless frame capture, renderer metrics, pointer dispatch, and
  task pumping APIs.
- Updated Lynx task runner creation so the ALL_ON_UI path reuses the UI runner
  when UI and TASM share the same loop.
- Added `@lynx-js/lynxtron-headless` with SDK helpers, CLI command, harness,
  TypeScript declarations, and a no-image smoke fixture.
- Tightened the harness to require `ready-to-show`, `on-first-screen`, and a
  software frame before screenshot acceptance.
- Added tap verification by writing `screenshot-after-tap.png` and requiring a
  changed PNG after input.

Verification:

- `ninja -C out/Debug lynxtron_app`: passed.
- `npm --prefix src/packages/lynxtron-headless test`: passed, 2 tests.
- `ninja -C out/Debug lynxtron_unittests`: passed.
- `/opt/homebrew/bin/timeout 90s ./out/Debug/lynxtron_unittests`: test launcher
  reported `SUCCESS: all tests passed` after its existing retry behavior.
- Acceptance smoke:

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

Acceptance smoke result:

- exit code `0`
- `status: success`
- `backend: windowless-software`
- `completionSignal: on-first-screen`
- `headless.headless: true`
- `headless.hasRenderer: true`
- `input.tap.changed: true`
- `screenshot.png`: PNG `1170 x 2532`, 24450 bytes
- `screenshot-after-tap.png`: PNG `1170 x 2532`, 22479 bytes

Artifacts:

- `/tmp/lynxtron-headless-acceptance/report.json`
- `/tmp/lynxtron-headless-acceptance/trace.jsonl`
- `/tmp/lynxtron-headless-acceptance/screenshot.png`
- `/tmp/lynxtron-headless-acceptance/screenshot-after-tap.png`

Blockers and risks:

- Image/texture-heavy bundles are not accepted yet. The current `default_app`
  path crashes in the software windowless image producer at
  `lynx/clay/gfx/image/image_producer.cc:566` with
  `unref_queue->GetContext()`.
- The current accepted fixture intentionally avoids image assets. HRT-011 tracks
  the follow-up needed before accepting image-heavy app bundles.
- JS NativeModules mock loading, locator APIs, protocol endpoint, Linux support,
  and external adapters remain out of the accepted slice.

Next action:

- User can verify the current smoke command and artifacts.
- Product decision needed next: prioritize HRT-011 image/texture support, or
  continue with HRT-009 JS mock loading for no-image bundles.

### MVP CDP Acceptance Update

Active task:

- Complete the scoped MVP according to the PM docs: true headless macOS runtime,
  runtime-local CDP control plane, CDP screenshot, CDP UI dump, CDP input, and
  complex smoke assertions.

Progress:

- Added the runtime-local CDP-compatible endpoint and protocol client.
- Exposed `Lynx.loadBundle`, lifecycle waits, `Page.takeScreenshot`,
  `Lynx.dumpUITree`, `Lynx.getHeadlessMetrics`, and
  `Input.dispatchTouchEvent` through the harness protocol.
- Added native windowless UI tree serialization as the internal backing for
  `Lynx.dumpUITree`.
- Kept native dump access internal; the durable observation API is CDP.
- Added a complex no-image Lynx fixture under
  `src/packages/lynxtron-headless/fixtures/complex-app/`.
- Updated the complex smoke so the tap target is derived from the CDP UI dump
  before dispatching CDP touch input.

Verification:

- `npm --prefix src/packages/lynxtron-headless test`: passed, 2 tests.
- `ninja -C out/Debug lynxtron_app`: passed.
- `npx rspeedy build` in
  `src/packages/lynxtron-headless/fixtures/complex-app`: passed,
  `dist/headless_complex.bundle` rebuilt.
- `ninja -C out/Debug lynxtron_unittests`: passed.
- `/opt/homebrew/bin/timeout 90s ./out/Debug/lynxtron_unittests`: final result
  `SUCCESS: all tests passed`; the launcher retried the existing
  `GlobalThreadTest.CurrentlyOn` first-run DCHECK crash.

Complex MVP smoke:

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

Complex smoke result:

- exit code `0`
- `status: success`
- `backend: windowless-software`
- `protocol.cdp.wsEndpoint`: present
- `headless.headless: true`
- `headless.hasRenderer: true`
- `input.tap.source: ui-dump`
- `input.tap.protocol: cdp`
- `input.tap.changed: true`
- before dump contains `Status idle`, `Selected Basic`, `Cart total $42`, and
  `Items 3`
- after dump contains `Status selected`, `Selected Pro`, `Cart total $57`, and
  `Items 4`

Complex smoke artifacts:

- `/tmp/lynxtron-headless-complex/report.json`
- `/tmp/lynxtron-headless-complex/trace.jsonl`
- `/tmp/lynxtron-headless-complex/screenshot.png`
- `/tmp/lynxtron-headless-complex/screenshot-after-tap.png`
- `/tmp/lynxtron-headless-complex/ui-dump.json`
- `/tmp/lynxtron-headless-complex/ui-dump-after-tap.json`

Baseline smoke was rerun through the CDP path and passed:

- `/tmp/lynxtron-headless-acceptance/report.json`: `status: success`
- before UI dump contains `tap 0`
- after UI dump contains `tap 1`
- `input.tap.protocol: cdp`
- `input.tap.changed: true`

Blockers and risks:

- No blocker for the scoped MVP.
- Image/texture-heavy bundles remain outside the MVP until HRT-011 is fixed.
- Linux CI/cloud, JS NativeModules mock loading, public locator helpers,
  Playwright adapter, and MCP tool/server remain follow-up work.

Next action:

- Use the complex smoke command and artifacts as the MVP acceptance baseline.
- Next product slice should be HRT-011 image/texture support or HRT-009 JS mock
  loading.

### Replay Recording Acceptance Update

Active task:

- Add recording and replay so a successful CDP MVP run can be reproduced from a
  machine-readable manifest.

Progress:

- Added `replay.json` as a first-class artifact.
- Added source/runtime/device/load metadata, bundle sha256, full recorded CDP
  action params, semantic tap target, result summary, and artifact hashes.
- Added SDK API `runReplay(manifestPath, options)`.
- Added CLI command `lynxtron-headless replay <replay.json>`.
- Added semantic replay mode, which re-finds the tap target from
  `Lynx.dumpUITree`.
- Added exact replay mode, which reuses the recorded touch coordinates.

Verification:

- `npm --prefix src/packages/lynxtron-headless test`: passed, 3 tests.

Record command:

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/complex-app/dist/headless_complex.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-record \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --smoke complex
```

Record result:

- exit code `0`
- `/tmp/lynxtron-headless-record/replay.json` written
- `replay.json` has `kind: lynxtron-headless-replay`
- `schemaVersion: 1`
- `replay.defaultMode: semantic`
- bundle sha256 recorded
- 11 CDP actions recorded
- semantic tap text recorded as `Upgrade plan`
- artifact hashes recorded

Semantic replay command:

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-replay-semantic
```

Semantic replay result:

- exit code `0`
- `status: success`
- `input.tap.source: ui-dump`
- before dump contains `Status idle`, `Selected Basic`, `Cart total $42`, and
  `Items 3`
- after dump contains `Status selected`, `Selected Pro`, `Cart total $57`, and
  `Items 4`

Exact replay command:

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-replay-exact \
  --mode exact
```

Exact replay result:

- exit code `0`
- `status: success`
- `input.tap.source: cli`
- before/after dump assertions match the semantic replay result

Blockers and risks:

- Replay currently covers the MVP run shape: one bundle/url load plus one
  recorded tap interaction. Multi-step arbitrary CDP replay remains future work.
- Semantic replay currently uses text target replay, not the full future locator
  model.

### Headed Authoring Playback Update

Active task:

- Add a visible authoring/debug mode for run and replay without changing the
  accepted CDP/windowless control plane.

Progress:

- Added public CLI/SDK option `headed`.
- Added public CLI/SDK option `slowMo`.
- Added internal harness flags `--headless-headed` and `--headless-slow-mo`.
- The harness shows a visible host window in authoring mode while preserving
  `headless: true` and the windowless renderer backing.
- Added slow-motion delays after key CDP actions.
- Added `authoring` metadata to `report.json` and `replay.json`.
- Added `authoring.headed-window` and `cdp.slow-mo` trace events.

Verification:

- `npm --prefix src/packages/lynxtron-headless test`: passed, 3 tests.

Headed replay command:

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-headed-replay \
  --headed \
  --slow-mo 50
```

Headed replay result:

- exit code `0`
- `status: success`
- `authoring.headed: true`
- `authoring.slowMoMs: 50`
- `authoring.rendererMode: windowless-renderer-visible-host`
- trace includes `authoring.headed-window`
- trace includes 6 `cdp.slow-mo` events
- `input.tap.source: ui-dump`
- before/after dump assertions passed

Headed run command:

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

Headed run result:

- exit code `0`
- `status: success`
- `authoring.headed: true`
- `authoring.slowMoMs: 10`
- trace includes `authoring.headed-window`
- trace includes 6 `cdp.slow-mo` events
- `replay.json` written

Blockers and risks:

- This is a visible authoring host over the windowless/CDP path. It does not
  prove ordinary GUI renderer CDP backing.
- Manual human operation recording was outside HRT-016 and is addressed by
  HRT-017's CDP recorder monitor.

### CDP Recorder Monitor Update

Active task:

- HRT-017: add a provider-neutral CDP input monitor, macOS Lynxtron provider
  bridge, `record` command, and recorded-action replay mode.

Progress:

- Added native macOS window input observation from `NSWindow.sendEvent` through
  `NativeWindowObserver::OnWindowInputEvent` and the JS `window-input` event.
- Added `LynxRecorder.start`, `LynxRecorder.stop`,
  `LynxRecorder.getEvents`, and
  `LynxRecorder.dispatchObservedInputEvent`.
- Added recorder events: `recordingStarted`, `inputEvent`,
  `actionRecorded`, `uiSnapshot`, `recordingStopped`, and `recordingError`.
- Added `lynxtron-headless record`.
- Added `recorded` replay mode that replays recorded
  `Input.dispatchTouchEvent` actions from `replay.json`.
- Updated `replay.json` so recorder runs use `replay.defaultMode: recorded`.

Verification:

- `npm --prefix src/packages/lynxtron-headless test`: passed, 5 tests.
- `ninja -C out/Debug lynxtron_app`: passed.
- Complex CDP smoke passed at
  `/tmp/lynxtron-headless-cdp-monitor-smoke`.
- CDP-provider injected record passed at
  `/tmp/lynxtron-headless-record-injected`.
- Recorded-action replay passed at
  `/tmp/lynxtron-headless-recorded-replay`.

Injected record result:

- exit code `0`
- `completionSignal: recorded-input`
- `input.recording.eventCount: 2`
- `input.recording.snapshotCount: 1`
- `input.recording.changed: true`
- `replay.defaultMode: recorded`

Recorded replay result:

- exit code `0`
- `completionSignal: recorded-replay`
- `input.tap.source: recorded-actions`
- `input.tap.actionCount: 2`
- `input.tap.changed: true`
- after dump contains `Status selected`, `Selected Pro`, `Cart total $57`,
  and `Items 4`

Blockers and risks:

- The macOS provider compile path is verified. Real human-click recording still
  needs a manual desktop check because automated OS input injection may require
  local Accessibility permissions.
- Mobile recording is not implemented yet, but the accepted provider schema is
  CDP-based and can be reused by a mobile provider.

### Agent Harness Skill Experiment

Experiment:

- HRT-018: test whether a no-background agent can use a small harness skill to
  build and validate a complex Lynx page.

Progress:

- Added `docs/headless-runtime/skills/lynxtron-headless-harness/SKILL.md`.
- Dispatched a worker agent with only the skill and a fixture requirement.
- Worker created `agent-decision-board` fixture and validation script.
- Worker built the bundle, ran headless harness with `--tap-text`, asserted
  before/after UI dumps, ran replay, and asserted replay output.

Independent review:

- Re-ran fixture build: passed.
- Re-ran headless harness at `/tmp/lynxtron-agent-decision-review`: passed.
- Re-ran before/after dump assertion: passed.
- Re-ran replay at `/tmp/lynxtron-agent-decision-review-replay`: passed.
- Re-ran replay after-dump assertion: passed.

Conclusion:

- The harness skill is sufficient for a no-background agent to create and
  verify a meaningful no-image Lynx fixture through the current harness.
- The skill should add a status-cadence note because the worker stayed silent
  long enough to require a nudge, even though the work eventually succeeded.

### Commerce Agent Comparison Experiment

Experiment:

- HRT-019: compare two agents implementing the same ecommerce Lynx page from a
  shared PRD and standard data mock.

Progress:

- Added `docs/headless-runtime/experiments/HRT-019-commerce-agent-comparison.md`.
- Added `docs/headless-runtime/experiments/HRT-019-commerce-standard-mock.ts`.
- Split acceptance into two smoke scenarios: product navigation and profile
  tab navigation.

Next action:

- Dispatch Agent A with the harness skill and owned headless fixture directory.
- Dispatch Agent B with the Lynx DevTool skill and owned shell-demo component
  directory.

Dispatch:

- Agent A (`commerce-harness-agent`) dispatched with the repo-local
  `lynxtron-headless-harness` skill.
- Agent B (`commerce-devtool-agent`) dispatched with the `lynx-devtool` skill
  and shell-demo context.

Review:

- Agent A completed and returned a full delivery note.
- PM re-ran Agent A build and both true-headless smoke scenarios:
  `/tmp/hrt-019-commerce-harness-detail-review` and
  `/tmp/hrt-019-commerce-harness-profile-review`, both passed.
- Agent A assertion script passed for scenario 1 and scenario 2.
- Agent A gap: the recommendation list is not backed by `scroll-view`, so the
  scrollability requirement is partial even though smoke states pass.
- Agent B wrote a mounted shell-demo component but did not return a final
  delivery note before the agent became unavailable.
- PM built the shell-demo app. `rspeedy build` passed, but `rspack build`
  failed on `rspack.config.ts` with `ERR_UNKNOWN_FILE_EXTENSION`.
- PM assembled `/tmp/hrt-019-commerce-devtool-app` from the existing desktop
  shell plus the newly built Lynx bundle, launched it with Release Lynxtron, and
  verified home, detail, and profile states through Lynx DevTool.
- Agent B screenshot evidence was captured through DevTool at
  `/var/folders/kw/95rr219x7yn1b29gnfxgjrbh0000gn/T/lynx-devtool-mcp-9Ovjbt/screenshot-Lynx_getScreenshot.jpeg`.

Conclusion:

- Harness path wins on repeatable agent/CI verification and clear artifacts.
- DevTool/shell path is useful for visual/headful inspection, but currently has
  weaker automation ergonomics because build/launch/viewport control are not
  standardized.

### Commerce Agent Comparison Fix Iteration

Experiment:

- HRT-019 follow-up: issue targeted fix requests for the current A and B gaps
  and compare execution efficiency again.

Planned fixes:

- Agent A: add a real scroll container to the headless fixture and prove it
  through the harness artifacts.
- Agent B: repair visual overflow and viewport/layout mismatch in the shell-demo
  component while preserving the same data and interactions.

Next action:

- Dispatch bounded workers against each existing implementation.

Review:

- Agent A completed autonomously with a delivery note and changed only the
  harness fixture scope.
- Agent A replaced the home recommendation container with `scroll-view` and
  added a focused `home-scroll` UI dump assertion.
- PM re-ran Agent A build and fresh headless smokes at
  `/tmp/hrt-019-commerce-harness-detail-fix-review2`,
  `/tmp/hrt-019-commerce-harness-profile-fix-review2`, and
  `/tmp/hrt-019-commerce-harness-home-scroll-fix-review2`; all required
  assertions passed.
- Agent A observation: one earlier PM rerun at
  `/tmp/hrt-019-commerce-harness-detail-fix-review` returned
  `INPUT_NO_VISUAL_CHANGE`; a clean rerun passed, so this is recorded as a tap
  stability observation rather than a failed fix.
- Agent B did not return a final delivery note before repeated wait timeouts.
- Agent B's current workspace result was still inspectable. PM verified that
  `rspeedy build` passed and produced the Lynx bundle, while the full build
  still failed on the existing Node 22 `rspack.config.ts` loader issue.
- PM assembled `/tmp/hrt-019-commerce-devtool-app-fix-review`, launched
  Release Lynxtron, and verified home, detail, and profile through DevTool.
- Agent B screenshots were saved to
  `/tmp/hrt-019-commerce-devtool-fix-review/home.jpeg`,
  `/tmp/hrt-019-commerce-devtool-fix-review/detail.jpeg`, and
  `/tmp/hrt-019-commerce-devtool-fix-review/profile.jpeg`.

Conclusion:

- Agent A is accepted for both product fix and autonomous execution efficiency.
- Agent B is partially accepted for visual outcome but not accepted as an
  autonomous delivery because PM had to complete the verification loop.
- The fix iteration reinforces the product direction: the headless harness gives
  a much tighter agent contract, while shell/devtool remains valuable for visual
  review but needs a standardized launch and screenshot harness before it can
  match the headless path on delegation efficiency.

### Lynx Open Interface E2E

Experiment:

- HRT-020: use the headless harness to verify generic Lynx open frontend
  interfaces from the nested `lynx/` checkout.

Confirmed scope:

- Target generic Lynx element capabilities, not explorer/showcase business UI.
- Use open `.d.ts` and CSS type sources only:
  `lynx/js_libraries/types/types/common/element/`,
  `lynx/js_libraries/types/types/main-thread/`,
  `lynx/js_libraries/type-element-api/types/element-api.d.ts`, and
  `lynx/tools/css_generator/index.d.ts`.
- Prioritize interaction, JavaScript API behavior, animation, and component
  interaction. CSS effect coverage can lag.
- Put the E2E test package parallel to `src/packages/lynxtron-headless`.
- Record skipped or blocked capabilities in
  `docs/headless-runtime/experiments/HRT-020-capability-gaps.md`.

Progress:

- Added `docs/headless-runtime/experiments/HRT-020-lynx-open-interface-e2e.md`.
- Added `docs/headless-runtime/experiments/HRT-020-capability-gaps.md`.

Next action:

- Inventory the interface files and dispatch the first implementation slice for
  `src/packages/lynxtron-interface-e2e/`.

Dispatch:

- Explorer `Kant` completed a read-only interface matrix covering P0/P1
  element rendering, interaction, public UI methods, image, input, textarea,
  scroll-view, list, text layout, animation, main-thread Element API, and core
  CSS effects.
- Worker `Leibniz` dispatched to implement the first E2E package under
  `src/packages/lynxtron-interface-e2e/`, with explicit instruction not to
  modify the nested `lynx/` checkout or existing harness/native code.

Review:

- Worker implemented `src/packages/lynxtron-interface-e2e/` with a standalone
  Lynx fixture package, smoke runner, and artifact assertion script.
- Covered first-slice behavior: basic `view`/`text` rendering and boxes,
  `bindtap` visible state update, controlled `input`/`textarea` state,
  `scroll-view` tag/attribute/state path, final animation state, and a small
  CSS effect gallery.
- PM re-ran `npm --prefix src/packages/lynxtron-interface-e2e run smoke`.
  The first sandboxed run failed during macOS AppKit initialization; the
  unrestricted rerun completed.
- Build emitted `dist/lynx_open_interface_e2e.bundle`.
- Headless harness run succeeded with `backend: windowless-software`,
  `status: success`, and `input.tap.changed: true`.
- `scripts/assert-ui.mjs` passed against visible texts, positive boxes,
  report fields, screenshots, and trace output.
- Artifacts were written to
  `src/packages/lynxtron-interface-e2e/artifacts/smoke/`.

Decision:

- Accept HRT-020 first slice as a harness-backed E2E proof for generic Lynx open
  interface behavior within the current harness capability envelope.
- Keep generated `dist/` and `artifacts/` local unless a later CI decision asks
  to check in golden artifacts.

Follow-up planning:

- HRT-021, HRT-022, HRT-023, and HRT-024 were added as protocol-first follow-up
  task cards.
- These capabilities must be implemented through CDP or Lynx custom CDP
  domains, not as platform-private test APIs.
- macOS Lynxtron, mobile explorer apps, and non-Lynxtron hosts should bridge
  into the same protocol method names so interface E2E remains cross-backend.

### HRT-025 Nested Scroll, List, And Text Selection Bug Hunt

Experiment:

- Extend the generic interface E2E fixture with nested `scroll-view`,
  selectable `text`, and attempted `list` control coverage.
- Use only public Lynx elements/props from the nested `lynx/` checkout and CDP
  input through the headless harness.

Validation:

- `npm --prefix src/packages/lynxtron-interface-e2e run build` passes.
- `Nested scroll row 4:0,-120` passes in
  `src/packages/lynxtron-interface-e2e/artifacts/nested-scroll-control-3`.
- `Standalone scroll selection drag target:0,-80` passes in
  `src/packages/lynxtron-interface-e2e/artifacts/standalone-selection-drag`.
- `Nested plain selection drag target:0,-80` passes in
  `src/packages/lynxtron-interface-e2e/artifacts/nested-plain-selection-drag`.
- `Selectable conflict text drag target:0,-80` had an earlier native abort with:

```text
Check failed: !delta.IsOrigin(). delta should not be empty
```

- After adding plain/standalone control targets, the latest custom selection
  drag passes in
  `src/packages/lynxtron-interface-e2e/artifacts/nested-custom-selection-drag-4`.

Findings:

- The custom selection crash is a real observed runtime abort, but not yet a
  stable minimized repro in the current fixture geometry.
- Plain `text-selection` in nested scroll and standalone scroll contexts passes.
- Adding `list` to the large smoke fixture exposed load-phase instability:
  first `SEGV_ACCERR` in `ListContainerView::UpdateContentOffsetForListContainer`,
  then a hang after aligning props with the existing `ScrollVirtualView` shape.

Next action:

- Create isolated fixtures for:
  - original custom selection geometry
  - list-only load and scroll
  - list plus selectable text after list-only load is stable
