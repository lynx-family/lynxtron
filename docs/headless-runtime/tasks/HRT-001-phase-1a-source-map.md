# HRT-001: Phase 1a Source-Level Feasibility Map

## Objective

Produce the source-level map needed to implement the smallest macOS true
headless runtime proof.

This task is a feasibility and patch-planning slice. It should remove ambiguity
before native runtime code changes begin.

## Context

The accepted product direction is documented in:

- `docs/headless-runtime/project-goal.md`
- `docs/headless-runtime/product-plan.md`
- `docs/headless-runtime/workflow.md`
- `docs/headless-runtime/engineering-design.md`

The first runtime proof must show:

```text
headless launch
  -> load fixture .lynx.bundle
  -> ready-to-show
  -> on-first-screen
  -> screenshot
  -> one tap
  -> report.json and trace.jsonl
```

## In Scope

- Identify exact source files and functions for:
  - headless launch flag and harness entry
  - `LynxWindow` or `LynxView` creation path
  - render active state and visibility coupling
  - first-screen and load/error events
  - screenshot or pixel capture path
  - DevTool input simulation path
  - minimal node/snapshot inspection path
- Propose the smallest implementation sequence for Phase 1a.
- Identify the narrowest build and smoke commands to validate the first patch.
- Record risks, stop conditions, and any source APIs that are missing.

## Out Of Scope

- Implementing the headless runtime patch.
- Designing the full SDK API.
- Implementing CLI packaging.
- Adding Playwright adapter or MCP tool code.
- Adding a NativeModules standard mock library.
- Creating a JSON mock DSL.

## File Ownership

This task should not modify product source files.

Allowed documentation output:

- `docs/headless-runtime/spikes/HRT-001-phase-1a-source-map.md`

The worker may read any repository source file needed for this map.

## Required Investigation Points

1. Where should the explicit headless runtime flag enter the process?
2. Can the existing default app loading path be reused for the harness, or
   should the headless harness be a separate package/app entry?
3. Which native object owns render active state and how should headless mode
   bypass user-visible window visibility?
4. What is the most direct source path for screenshot capture from the Lynx
   render surface?
5. Which existing DevTool input simulation APIs can satisfy tap and scroll?
6. What is the minimal event trace source for `ready-to-show`,
   `on-first-screen`, load errors, and runtime errors?
7. What fixture bundle or existing spec can verify tap and first screen?
8. What build target and launch command should be used on macOS?

## Acceptance Criteria

- Output markdown exists at
  `docs/headless-runtime/spikes/HRT-001-phase-1a-source-map.md`.
- The map cites concrete files, functions, classes, and line numbers where
  possible.
- The proposed Phase 1a implementation is split into at least three small
  follow-up tasks.
- Each follow-up task has file ownership and a narrow verification method.
- Missing APIs are explicitly called out as blockers or risks.
- No product source files are modified.

## Verification

Required verification for this task:

```bash
rg -n "headless|first-screen|ready-to-show|SetEventSimulationProxy|IsVisible|capture|screenshot" src
```

Additional verification should include whichever source navigation commands the
worker used to support the map.

No build is required for this documentation-only feasibility slice.

## Delivery Note Format

The worker must report:

- Summary.
- Output file path.
- Key source findings.
- Proposed follow-up tasks.
- Verification commands run.
- Blockers and risks.

## Stop Conditions

Stop and report a blocker if:

- The render active path cannot be traced from source.
- No plausible screenshot or render surface capture path can be found.
- The macOS build or runtime target cannot be identified.
- The task needs product code changes to answer the feasibility questions.
