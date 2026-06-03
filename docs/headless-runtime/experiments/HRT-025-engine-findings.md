# HRT-025 Engine Findings

This document records behavior mismatches found by the harness-backed Lynx open
interface E2E bug hunt.

## F01: `capture-bindtap` Does Not Fire In The Event Matrix

Status: Open.

Area:

- Lynx event propagation.

Public interface source:

```text
lynx/js_libraries/types/types/common/events.d.ts
```

The public `TapProps` interface includes:

```text
'capture-bindtap'?: LynxBindCatchEvent<T>['Tap']
```

Fixture:

- `src/packages/lynxtron-interface-e2e/src/app.tsx`
- Section: `Interaction bug hunt lab`
- Target text: `Event matrix inner action`

Fixture structure:

- Outer `view`
  - `capture-bindtap={() => addEventLog('outer-capture')}`
  - `bindtap={() => addEventLog('outer-bind')}`
- Middle `view`
  - `bindtap={() => addEventLog('middle-bind')}`
- Inner `view`
  - `bindtap={() => addEventLog('inner-bind')}`

Expected visible result after tapping the inner view:

```text
event matrix result outer-capture>inner-bind>middle-bind>outer-bind
```

Actual visible result:

```text
event matrix result inner-bind>middle-bind>outer-bind
```

Evidence:

- Command:

```sh
npm --prefix src/packages/lynxtron-interface-e2e run smoke
```

- Runtime:
  - platform: macOS
  - backend: `windowless-software`
  - viewport: 390x844
- Bundle:
  - `src/packages/lynxtron-interface-e2e/dist/lynx_open_interface_e2e.bundle`
  - latest observed size: 118.0 kB
- Artifacts:
  - `src/packages/lynxtron-interface-e2e/artifacts/smoke/report.json`
  - `src/packages/lynxtron-interface-e2e/artifacts/smoke/trace.jsonl`
  - `src/packages/lynxtron-interface-e2e/artifacts/smoke/ui-dump-after-tap.json`
  - `src/packages/lynxtron-interface-e2e/artifacts/smoke/ui-snapshot-after-tap.json`

Why this is treated as an engine/open-interface mismatch:

- The target is in the visible first-screen interaction lab.
- `Input.dispatchTouchEvent` is accepted through CDP.
- The tap produces a UI dump change.
- The same tap path proves normal bubbling:
  `inner-bind>middle-bind>outer-bind`.
- The missing segment is specifically the capture handler declared by the open
  frontend type interface.

Other cases in the same run:

- `bindtap` nested bubbling passes.
- `catchtap` nested stop behavior passes.
- `scroll-view` clipped child interaction passes through
  `LynxInput.scrollIntoView`.
- Keyed reorder passes:
  `Key item charlie`, `Key item alpha`, `Key item delta`.
- Controlled form state passes.

Next triage:

- Confirm whether `capture-bindtap` is expected to be supported by this PC
  runtime path.
- If supported, debug event dispatch/capture registration in the Lynx runtime.
- If intentionally unsupported on PC, update the public compatibility contract
  or add an explicit documented capability gap.

## F02: Dragging Custom Selectable Text Inside Nested Scroll Context Crashed ScrollView

Status: Needs stable minimization.

Area:

- Text selection gesture.
- Nested scroll / scroll conflict handling.

Public interface sources:

```text
lynx/js_libraries/types/types/common/element/text.d.ts
lynx/js_libraries/types/types/common/element/scroll-view.d.ts
```

Relevant public props:

- `text-selection`
- `custom-text-selection`
- `bindselectionchange`
- `scroll-view` `bindscroll`

Fixture:

- `src/packages/lynxtron-interface-e2e/src/app.tsx`
- Section: `Nested scroll and selection conflict`
- Target text: `Selectable conflict text drag target`

Fixture structure:

- Outer `scroll-view`
  - `bindscroll={() => setOuterScrollCount(...)}`
- Inner `scroll-view`
  - `bindscroll={() => setInnerScrollCount(...)}`
- Selectable `text` inside the outer scroll content
  - `text-selection={true}`
  - `custom-text-selection={true}`
  - `bindselectionchange={() => setSelectionChangeCount(...)}`
  - `bindtap={() => setSelectionTapCount(...)}`

Repro command:

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-interface-e2e/dist/lynx_open_interface_e2e.bundle \
  --artifact-dir src/packages/lynxtron-interface-e2e/artifacts/selection-drag-flow \
  --tap-text 'Run interaction smoke' \
  --tap-text 'Event matrix inner action' \
  --tap-text 'Nested child action' \
  --tap-text 'Catch child action' \
  --tap-text 'Scroll row 4' \
  --tap-text 'Reorder keyed items' \
  --tap-text 'Prime form state' \
  --drag-text 'Selectable conflict text drag target:0,-80' \
  --width 390 \
  --height 844 \
  --timeout 10000 \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron
```

Observed result in the original failing run:

- The Lynxtron process aborts with signal 6.
- Fatal log:

```text
[lynx] [FATAL:scroll_view.cc(683)] Check failed: !delta.IsOrigin(). delta should not be empty
```

Key stack frames:

```text
clay::ScrollView::DidScroll()
clay::NestedScrollable::DoScroll(...)
clay::ScrollView::DoScroll(...)
clay::NestedScrollable::HandleNestedScroll(...)
clay::NestedScrollManager::DispatchScroll(...)
clay::NestedScrollManager::OnDynamicAnimationUpdate(...)
```

Control and follow-up runs:

- `Nested scroll row 4:0,-120` after the same tap setup succeeds.
  - `dragSequence[0].changed === true`
  - visible text reports `nested scroll outer 0 inner 21`
  - this indicates the inner scroll route can work without crashing.
- `Standalone scroll selection drag target:0,-80` succeeds.
  - This narrows the issue away from plain selectable text inside a single
    scroll container.
- `Nested plain selection drag target:0,-80` succeeds.
  - This narrows the issue away from plain `text-selection` in the nested
    scroll section.
- After adding the plain and standalone control targets, the current fixture no
  longer crashes on `Selectable conflict text drag target:0,-80`; it records a
  successful CDP drag with `changed === true`.

Why this is still treated as an engine/runtime candidate:

- The original failure was a runtime abort on a CHECK, not a harness assertion.
- Input is dispatched through the same CDP touch path used by other passing
  interactions.
- The crash happens in scroll/nested-scroll engine code during animation frame
  handling.
- A user drag on selectable text should not be able to abort the process even
  if selection is unsupported on PC.

Next triage:

- Rebuild a smaller fixture matching the original failing geometry:
  outer `scroll-view`, inner `scroll-view`, one `text-selection` +
  `custom-text-selection` target, and one bottom spacer.
- Keep the current control artifacts:
  - `artifacts/nested-scroll-control-3`
  - `artifacts/standalone-selection-drag`
  - `artifacts/nested-plain-selection-drag`
  - `artifacts/nested-custom-selection-drag-4`
- In runtime code, guard zero scroll delta before `ScrollView::DidScroll()` or
  avoid scheduling nested scroll dynamic animation when no effective delta is
  present.

## F03: `list` Control Fixture Caused Load-Phase Instability

Status: Needs isolation.

Area:

- `list` rendering and scroll container initialization on PC/headless.

Public interface sources:

```text
lynx/js_libraries/types/types/common/element/list.d.ts
lynx/js_libraries/types/types/common/element/list-item.d.ts
```

Observed while adding a `list` selection control to the same E2E fixture:

- First run crashed during template load with `SEGV_ACCERR`.
- Key stack frames included:

```text
clay::ListContainerView::UpdateContentOffsetForListContainer(...)
clay::ListContainerWrapper::UpdateContentOffsetForListContainer(...)
lynx::tasm::PaintingContextClayRef::UpdateContentOffsetForListContainer(...)
lynx::list::LinearLayoutManager::HandleLayoutOrScrollResult(...)
lynx::list::ListContainerImpl::OnLayoutChildren(...)
```

- After aligning the fixture props with the existing `ScrollVirtualView` usage
  (`span-count`, `need-layout-complete-info`, `scroll-bar-enable`), the same
  load check did not immediately crash but hung until the process had to be
  killed manually.

Why this is not yet a confirmed product bug:

- The `list` control was temporarily added to the large smoke fixture, not an
  isolated list-only fixture.
- The current evidence proves instability, but not the smallest valid public
  interface input that triggers it.

Next triage:

- Create a separate list-only fixture or runtime mode with:
  - a fixed-height `list`
  - `list-item` children with mandatory `item-key`
  - no selection text
- Add `text-selection` only after the list-only load path is stable.
