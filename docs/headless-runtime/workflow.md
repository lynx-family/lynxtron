# Workflow

## Operating Model

This project is run in PM-driven mode.

- The PM owns product scope, phase plan, task cards, acceptance rules, review,
  and status documents.
- Engineering work is performed through bounded tasks with explicit file
  ownership and verification requirements.
- The PM does not directly implement product code unless the user explicitly
  overrides the PM operating model.
- Every meaningful product decision, blocker, scope change, or acceptance
  result must be recorded under `docs/headless-runtime/`.

## Roles And Responsibilities

PM:

- Maintain `project-goal.md`, `product-plan.md`, `workflow.md`, and
  `status-log.md`.
- Keep `engineering-design.md` aligned with accepted decisions.
- Select the next task and define acceptance criteria.
- Review implementation and verification evidence before acceptance.

Worker:

- Own the assigned files or investigation scope.
- Avoid reverting unrelated changes.
- Keep changes scoped to the task card.
- Provide changed file paths, validation commands, outputs, blockers, and
  follow-up risks in the delivery note.

User:

- Resolve product decisions that are not inferable from the source or existing
  docs.
- Accept or redirect phase priorities when scope changes.

## Verification

Documentation verification:

- Confirm the expected markdown files exist.
- Confirm the plan references the source-backed design.
- Confirm each task has scope, ownership, acceptance, and verification.

Code verification, once product code starts:

- Run the narrowest build or test that covers the changed files.
- For native/runtime changes, record the exact build target and runtime command.
- For SDK/CLI changes, run TypeScript/build checks and at least one smoke
  command.
- For headless runtime changes, collect screenshot, `report.json`, and
  `trace.jsonl` whenever the task reaches runtime execution.

MVP required CDP smoke:

```text
launch macOS headless runtime
  -> start runtime-local CDP-compatible endpoint
  -> Lynx.loadBundle
  -> Lynx.waitForReadyToShow
  -> Lynx.waitForFirstScreen
  -> Page.takeScreenshot
  -> Lynx.dumpUITree
  -> derive tap target from the UI dump
  -> Input.dispatchTouchEvent
  -> Lynx.waitForFrameAfter
  -> Lynx.dumpUITree
  -> assert after-dump UI state changed
  -> write screenshot, after screenshot, before/after UI dumps, report.json,
     trace.jsonl, and replay.json
  -> replay replay.json in semantic mode
  -> replay replay.json in exact mode
  -> optionally replay with --headed --slow-mo for authoring/debug visibility
  -> exit with stable code
```

Baseline Phase 1a smoke:

```text
launch macOS headless runtime
  -> load fixture .lynx.bundle
  -> observe ready-to-show
  -> observe on-first-screen
  -> capture screenshot
  -> dispatch one tap
  -> write report.json and trace.jsonl
  -> exit with stable code
```

Current MVP acceptance command:

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

Current baseline acceptance command:

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

Current MVP acceptance requires:

- process exit code `0`
- `report.json` with `backend: "windowless-software"`,
  `headless.headless: true`, `headless.hasRenderer: true`,
  `protocol.cdp.wsEndpoint`, and `input.tap.protocol: "cdp"`
- `trace.jsonl` with CDP request/response entries for `Lynx.loadBundle`,
  `Lynx.waitForReadyToShow`, `Lynx.waitForFirstScreen`,
  `Page.takeScreenshot`, `Lynx.dumpUITree`, `Input.dispatchTouchEvent`, and
  `Lynx.waitForFrameAfter`
- `trace.jsonl` with `artifact.screenshot`, `artifact.ui-dump`,
  `input.tap-target`, `input.pointer`, `artifact.ui-dump-after-tap`, and
  `input.tap`
- `screenshot.png` and `screenshot-after-tap.png` written as PNG files
- `ui-dump.json` and `ui-dump-after-tap.json` written as JSON files
- `replay.json` written with source hash, runtime metadata, device profile,
  load options, CDP action params, semantic tap target, and artifact hashes
- complex smoke before dump contains `Selected Basic`, `Cart total $42`,
  `Status idle`, and `Items 3`
- complex smoke after dump contains `Selected Pro`, `Cart total $57`,
  `Status selected`, and `Items 4`
- `input.tap.source: "ui-dump"` in the complex smoke
- `input.tap.changed: true`
- `lynxtron-headless replay <replay.json>` passes in semantic mode
- `lynxtron-headless replay <replay.json> --mode exact` passes in exact mode
- `lynxtron-headless replay <replay.json> --headed --slow-mo <ms>` passes and
  records `authoring.headed: true`
- `lynxtron-headless record <bundle>` can start `LynxRecorder`, accept observed
  input events, write `input.recording` metadata, and produce a
  `replay.defaultMode: recorded` manifest
- `lynxtron-headless replay <recorded replay.json>` passes recorded-action
  replay and preserves the expected after-dump state for the complex fixture
- no product acceptance based on hidden native-window rendering

## Acceptance

A task can be accepted only when:

- Scope matches the task card.
- No unrelated files were modified.
- Source claims are backed by file paths and symbols.
- Verification ran successfully, or blocked verification is recorded with the
  exact reason.
- Artifacts required by the task are present and reviewable.
- The status log and product plan are updated.

Documentation-only tasks can be accepted when:

- Markdown files exist in the expected location.
- They align with accepted product decisions.
- They identify unresolved questions explicitly.
- They do not overwrite unrelated existing docs.

## Commit Policy

- Keep commits small and phase-aligned.
- Do not mix PM docs, native runtime work, SDK work, and CLI work in one commit
  unless the task card explicitly requires it.
- Prefer one task card per commit.
- Commit messages should include the task id, for example `HRT-001: map phase
  1a headless sources`.

## Delegation Rules

Each handoff must include:

- Task id and title.
- Objective.
- In scope and out of scope.
- File ownership or investigation boundaries.
- Required verification.
- Expected delivery note format.
- Known risks and stop conditions.

Worker delivery notes must include:

- Summary of changes or findings.
- Files changed.
- Verification commands and results.
- Artifacts produced.
- Blockers or follow-up risks.

## Blocker Handling

Record a blocker when:

- A required source path or API cannot be found.
- A runtime behavior cannot be verified locally.
- A product decision changes acceptance criteria.
- A build or smoke command is required but cannot run in the current
  environment.
- A task needs broader file ownership than assigned.

Blocker reports must include:

- What is blocked.
- Why it is blocked.
- Evidence collected.
- Decision or permission needed.
- Fallback options.

## Status Updates

Update `status-log.md` after:

- PM document changes.
- Task dispatch.
- Task acceptance or rejection.
- Verification success or failure.
- Blocker discovery.
- Scope or priority change.

User-facing updates should be short. The markdown record should be complete.
