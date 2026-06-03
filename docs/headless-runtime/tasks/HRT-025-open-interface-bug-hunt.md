# HRT-025 Open Interface Bug-Hunt E2E

## Objective

Expand `src/packages/lynxtron-interface-e2e` with higher-complexity Lynx open
interface cases until it finds real PC rendering, interaction, or JavaScript
API defects. When the missing part is a harness/runtime control-plane feature,
implement it through CDP or a Lynx custom CDP domain instead of adding
test-private native hooks.

## Product Rule

All reusable observation and interaction features must be protocol-facing:

- use standard CDP methods when they match the behavior
- use Lynx custom CDP domains when Lynx-specific state is needed
- keep macOS Lynxtron, mobile explorer apps, and non-Lynxtron bridges behind
  the same protocol method names

## Current Green Baseline

The last accepted HRT-020 green baseline passes with:

1. `Run interaction smoke`
2. `Nested child action`
3. `Catch child action`
4. `Scroll row 4`
5. `Prime form state`

Assertions cover:

- nested `bindtap` bubbling
- nested `catchtap` stop behavior
- scroll-view child clipping recovery through `LynxInput.scrollIntoView`
- controlled `input`/`textarea` programmatic state
- scroll-view rendering and initial state
- animation final state
- screenshot, raw UI dump, normalized `LynxSnapshot.capture`, report, and trace

## First Finding

Adding `Scroll row 4` to the tap sequence found HRT-020-G06:

- UI dump exposed the action candidate.
- The target coordinate was outside the nearest `scroll-view` clipping box.
- `Input.dispatchTouchEvent` completed but no visual/UI change occurred.
- The harness returned `INPUT_NO_VISUAL_CHANGE`.

This is now initially fixed for macOS Lynxtron headless by
`LynxInput.scrollIntoView`. The method detects scroll ancestor clipping, sends
CDP-backed touch move events inside the scroll container, re-dumps the UI, and
then taps the updated target. The green report records `scrolled: true` and
`scrollCount: 1` for `Scroll row 4`.

## Second Finding

Adding the interaction bug-hunt lab found an open event propagation mismatch:

- Public interface source declares `capture-bindtap`.
- The fixture attaches `capture-bindtap` to an outer `view` and `bindtap` to
  outer/middle/inner views.
- Tapping the visible `Event matrix inner action` target produces:

```text
event matrix result inner-bind>middle-bind>outer-bind
```

- The expected result is:

```text
event matrix result outer-capture>inner-bind>middle-bind>outer-bind
```

The same run proves the harness path is working:

- all seven CDP semantic taps record `changed: true`
- nested `bindtap` passes
- nested `catchtap` passes
- scroll-view child clipping passes through `LynxInput.scrollIntoView`
- keyed reorder passes
- controlled form state passes

The finding is recorded in:

```text
docs/headless-runtime/experiments/HRT-025-engine-findings.md
```

## Third Finding

Adding drag gestures found a process crash candidate in selectable text plus
nested scroll conflict handling:

- Added `--drag-text <text>:<dx>,<dy>` to the harness/CLI/SDK.
- Added `Nested scroll and selection conflict` fixture coverage.
- Control run: dragging `Nested scroll row 4` succeeds and reports:

```text
nested scroll outer 0 inner 21
```

- Original failing run: dragging `Selectable conflict text drag target` crashed
  the Lynxtron process with:

```text
Check failed: !delta.IsOrigin(). delta should not be empty
```

Follow-up control runs refined the boundary:

- dragging `Nested scroll row 4` succeeds
- dragging `Standalone scroll selection drag target` succeeds
- dragging `Nested plain selection drag target` succeeds
- with the latest control fixture geometry, dragging
  `Selectable conflict text drag target` also succeeds, so the original native
  abort is real evidence but still needs a stable minimized fixture

This finding is recorded in:

```text
docs/headless-runtime/experiments/HRT-025-engine-findings.md
```

## Fourth Finding

Adding a `list` selection control to the large fixture exposed list load-phase
instability:

- initial run hit `SEGV_ACCERR`
- stack entered `ListContainerView::UpdateContentOffsetForListContainer`
- after aligning props with the existing `ScrollVirtualView` pattern, the load
  check no longer immediately crashed but hung until the runtime process was
  manually killed

This is recorded as an isolation-needed candidate in:

```text
docs/headless-runtime/experiments/HRT-025-engine-findings.md
```

## Next Case Matrix

| Priority | Case | Expected Result | If It Fails |
| --- | --- | --- | --- |
| P0 | Scroll-view clipped row interaction | Protocol scrolls or drags into view, then tap updates visible row state. | Initial macOS Lynxtron fix is green through `LynxInput.scrollIntoView`; expand to more rows and nested containers. |
| P0 | Page-level scroll into view | Page or root scroll behavior moves offscreen content into an actionable area. | If root/page scroll is unsupported in this runtime, record it separately from scroll-view child clipping. |
| P0 | Input focus plus protocol text insertion | `Input.insertText` updates controlled input/textarea state. | Implement HRT-021. |
| P0 | Conditional render and reorder after tap | New/reordered nodes appear in snapshot without fragile path assertions. | Record runtime diff with before/after snapshot and trace. |
| P0 | Event propagation matrix | `bindtap`, `catchtap`, capture/global variants match visible state expectations. | Separate fixture expectation bug from runtime event-order bug. |
| P1 | List render and scroll basics | Keyed item render remains stable after update/scroll. | Record virtualization or list method gap. |
| P1 | Animation frame observation | Final state passes; intermediate frames are sampled when HRT-022 exists. | Implement HRT-022 before asserting timing-sensitive frames. |
| P1 | Image load/error | Load/error events update visible state and screenshot remains nonblank. | Record image/texture runtime gap or fixture asset issue. |

## Acceptance

- Every new reusable capability is exposed through CDP/custom CDP.
- Tests use public Lynx element/CSS/JS interfaces from `lynx/`.
- A failing case must leave a reproducible artifact set:
  `report.json`, `trace.jsonl`, screenshot(s), UI dump(s), and snapshot(s).
- Harness gaps are either implemented or recorded in
  `docs/headless-runtime/experiments/HRT-020-capability-gaps.md`.
- Runtime behavior bugs are recorded with exact command, bundle, viewport,
  failing assertion, and observed artifact evidence.
