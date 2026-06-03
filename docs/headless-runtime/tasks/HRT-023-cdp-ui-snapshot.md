# HRT-023 CDP UI Snapshot

## Objective

Add a normalized UI snapshot artifact through CDP. The snapshot describes the
visible page shape and operation affordances in a screenshot-correlatable way.
It is not a business semantic extractor.

## Product Requirement

Expose:

```text
LynxSnapshot.capture
```

The method derives a stable snapshot from CDP UI dump, screenshot metadata, and
available backend box/text/action data.

## Snapshot Shape

First version should include:

- viewport
- visible text runs
- visual blocks
- action candidates
- scroll containers
- repeated collection candidates when detectable
- visual health checks:
  - blank/near-empty screen
  - obvious text overlap
  - offscreen visible text
  - tiny action targets

## Backend Model

```text
CDP UI dump / screenshot / provider node metadata
  -> normalized snapshot builder
  -> ui-snapshot.json artifact
```

Backends can vary in how they provide node metadata, but the output contract
should remain stable across macOS Lynxtron, mobile explorer apps, and
non-Lynxtron host bridges.

## Acceptance

- A harness run can emit `ui-snapshot.json` next to `ui-dump.json`.
- HRT-020 can assert action labels, positive boxes, and basic visual health from
  the snapshot.
- Snapshot includes evidence mapping back to node ids/boxes where available.
- Missing backend metadata is represented as absent/unknown, not fabricated.

## Implementation Status

Status: Initial done on macOS Lynxtron headless.

Implemented protocol method:

```text
LynxSnapshot.capture
```

Implemented artifacts:

- `ui-snapshot.json`
- `ui-snapshot-after-tap.json`

Implemented first-version schema:

- `schemaVersion`
- `protocol`
- `method`
- `source`
- `viewport`
- `nodeCount`
- `documentTextRuns`
- `visibleTextRuns`
- `visualBlocks`
- `actionCandidates`
- `scrollContainers`
- `repeatedCollectionCandidates`
- `visualHealth`

The first version derives from `Lynx.dumpUITree` inside the CDP method. It does
not expose the native dump as a separate public test API.

## Verification

Commands:

```sh
npm --prefix src/packages/lynxtron-headless test
npm --prefix src/packages/lynxtron-interface-e2e run smoke
```

Result:

- SDK tests passed: 5 tests.
- HRT-020 smoke passed through true headless macOS Lynxtron.
- HRT-020 assertion consumed `ui-snapshot.json` and
  `ui-snapshot-after-tap.json`.
- Snapshot protocol fields asserted:
  - `protocol: cdp`
  - `method: LynxSnapshot.capture`
- Snapshot action candidate assertion found `Run interaction smoke` with a
  positive box.
- Snapshot visual health reported non-blank before and after interaction.

## Known Limits

- Current backend is macOS Lynxtron windowless software rendering.
- Mobile explorer and non-Lynxtron bridge providers are not implemented yet.
  They must expose equivalent dump/screenshot/provider metadata behind the same
  CDP method.
- Tiny action target and richer overlap diagnostics are intentionally narrow in
  the first version.
- `visibleTextRuns` is viewport/screenshot-correlatable; full-page semantic
  text is available through `documentTextRuns`.
