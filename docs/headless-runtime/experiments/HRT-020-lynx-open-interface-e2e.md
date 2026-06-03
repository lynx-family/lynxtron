# HRT-020 Lynx Open Interface E2E Plan

## Objective

Use the headless harness to build E2E tests for generic Lynx open frontend
interfaces defined under:

```text
lynx/js_libraries/types/types/common/element/
lynx/js_libraries/types/types/main-thread/
lynx/js_libraries/type-element-api/types/element-api.d.ts
lynx/tools/css_generator/index.d.ts
```

The purpose is to verify that real Lynxtron runtime behavior matches the
public element, JavaScript, interaction, animation, and CSS interface surface.
The test source must be written against the open frontend interface only. It
must not depend on internal native implementation details.

## Confirmed Product Decisions

- Target scope is generic Lynx element capability, not explorer/showcase
  business UI.
- Element coverage starts with `view`, `text`, `image`, `scroll-view`, `list`,
  `input`, and `textarea`.
- CSS coverage can lag behind the interaction and JavaScript API coverage.
- Interaction, JavaScript interface behavior, animation behavior, and component
  interaction are priority areas.
- Assertions should use screenshot, UI dump, report, trace, and behavior
  evidence. They should not rely on fragile DOM path matching.
- Animation cases need explicit treatment: assertions must account for timing,
  intermediate frames, final state, and screenshots before/after animation
  where the harness supports it.
- Tests should live in a package or directory parallel to
  `src/packages/lynxtron-headless`, not inside `lynxtron-headless/fixtures`.
- The first implementation may hand-write representative fixture pages from
  the interface definitions. Automatic generation from `.d.ts` can come later.
- Missing harness/runtime capability should be skipped, not faked, and recorded
  in the capability gap document.

## Proposed Location

```text
src/packages/lynxtron-interface-e2e/
```

This package should use `@lynx-js/lynxtron-headless` as the execution harness.
It should avoid owning harness runtime code unless a reusable harness feature is
needed and separately accepted.

Expected first structure:

```text
src/packages/lynxtron-interface-e2e/
  package.json
  lynx.config.mjs
  src/
    app.tsx
    styles.css
    cases/
      element-basics.tsx
      interaction.tsx
      animation.tsx
      form-controls.tsx
      scroll-and-list.tsx
  scripts/
    assert-ui.mjs
    run-smoke.mjs
```

## Interface Sources

Primary sources:

- Element declarations:
  `lynx/js_libraries/types/types/common/element/*.d.ts`
- Main-thread element API:
  `lynx/js_libraries/types/types/main-thread/element.d.ts`
- Global element API:
  `lynx/js_libraries/type-element-api/types/element-api.d.ts`
- CSS property type definitions:
  `lynx/tools/css_generator/index.d.ts`

Non-goals:

- Do not read C++ implementation files to decide expected frontend behavior.
- Do not use private harness/native methods directly from fixture code.
- Do not validate explorer/showcase product UI unless a future task explicitly
  targets those apps.

## First Test Matrix

### Priority Matrix

| Priority | Capability | Public interface source | Fixture strategy | Harness assertion | First-slice risk |
| --- | --- | --- | --- | --- | --- |
| P0 | Basic element rendering and attributes | `common/element/element.d.ts`, `view.d.ts`, `text.d.ts`, `props.d.ts`, `attributes.d.ts` | Render nested `view`, `text`, raw text, id/class/style/data markers. | UI dump contains node types, text, and non-empty boxes; screenshot is nonblank. | UI dump may not expose class/style/data directly; assert visible text and boxes when metadata is unavailable. |
| P0 | Tap/touch interaction and event-visible state | `common/events.d.ts`, `props.d.ts` | Tappable view updates visible text with event type, target id, dataset, and count. | Use `--tap-text`; assert after UI dump has updated event text and changed frame. | Long press, full capture/bubble chain, and gesture variants are out of first slice. |
| P0 | Public UI methods from fixture code | `common/element/methods.d.ts` | Fixture invokes public methods such as bounding rect or scroll methods and renders result text. | Assert rendered result contains positive dimensions/offsets and visible state change. | Harness should not call private invoke/native APIs; fixture must expose results visibly. |
| P0 | Image load/error basics | `common/element/image.d.ts`, `common/events.d.ts` | Use local/base64 valid image and invalid source; load/error events update visible text. | Assert `image load ok` and `image error ok`; screenshot nonblank. | Known image/texture gap may block this; record in gap doc if it fails for runtime reasons. |
| P0 | Input programmatic value/focus/selection | `common/element/input.d.ts` | Tappable control triggers public methods or state path to set/get value, focus/blur, and selection result text. | Assert value/focus/selection text after tap. | Current harness lacks keyboard/IME input; do not test real typing in first slice. |
| P0 | Textarea programmatic multiline value/focus/selection | `common/element/textarea.d.ts` | Same as input with multiline value and maxline/maxlength visible state. | Assert multiline value and method/event result text. | Real soft keyboard and confirm-key behavior are out of first slice. |
| P0 | Scroll-view method/event behavior | `common/element/scroll-view.d.ts`, `common/element/common.d.ts` | Fixed-height scroll-view with many children; button invokes scroll and renders scroll info. | Assert scroll info text and later item visibility or changed screenshot. | Gesture drag is secondary; prefer fixture-driven public methods first. |
| P0 | List render and scroll basics | `common/element/list.d.ts`, `list-item.d.ts` | Fixed-height list with keyed items; button scrolls to index/key and renders visible cell info. | Assert target item or visible cell info appears. | Return shape for `getVisibleCells` needs probing; waterfall/sticky are later. |
| P1 | Text layout and bounding APIs | `common/element/text.d.ts`, `common/events.d.ts` | Fixed-width long text, maxline, layout event, bounding rect result text. | Assert line/bounds result is positive and stable enough. | Text selection gestures are platform-sensitive and later. |
| P1 | CSS transition/animation and `Element.animate()` | `common/events.d.ts`, `props.d.ts`, `main-thread/element.d.ts`, `main-thread/animation.d.ts`, `css_generator/index.d.ts` | Trigger animation/transition; render event log and final state text. | Assert event order or final state plus screenshot/frame change. | No precise timing assertion; intermediate frames are skipped unless harness grows support. |
| P1 | Main-thread Element API | `main-thread/element.d.ts` | Fixture uses public query/set/style/invoke/animate API and renders result text or visual state. | Assert visible result text or box/screenshot effect. | Need prove build/runtime path supports main-thread API; `type-element-api` `__*` globals are not test expectations. |
| P1 | Core CSS visible effects | `tools/css_generator/index.d.ts`, `common/props.d.ts` | Small style gallery for width/height, padding/margin, flex, background, color, opacity, transform. | Assert boxes/screenshot do not collapse and selected effects are visible. | CSS coverage can lag; no full computed-style assertions yet. |

### Element Rendering

Representative coverage:

- `view`: layout container, nested children, visibility, box dimensions.
- `text`: raw text, multi-text layout, font size/color/weight where observable.
- `image`: basic image node presence and fallback behavior. Image-heavy native
  rendering gaps may be skipped and recorded.
- `scroll-view`: vertical scroll container existence and scroll interaction
  where input support is available.
- `list`: repeated item rendering and stable visible item count. Virtualization
  details are not a first-round assertion unless exposed by public API.
- `input`: value display, focus/tap, change/input events if supported.
- `textarea`: multiline value display and change/input events if supported.

### Interaction And JavaScript API

Representative coverage:

- `bindtap`/tap event changes visible UI state.
- Parent/child interaction does not lose the target action.
- Event payload or state update is reflected in visible text.
- Public JS element APIs that are visible through the open `.d.ts` surface are
  invoked from fixture code and validated through UI changes or trace output.
- Unsupported or unavailable API effects are skipped and logged in the gap
  document.

### Animation

Representative coverage:

- Simple transition or animation starts from a deterministic initial state.
- Harness captures before/after screenshots and UI dumps.
- Final visual/state result is asserted.
- If intermediate frame sampling or computed animation state is not available,
  record the gap and only assert final visible effect.

### Component Interaction

Representative coverage:

- Nested component state update after user input.
- Sibling component update after shared state change.
- Conditional render after interaction.
- Repeated children/list item update after interaction.

### CSS Effect Coverage

First-round CSS coverage is narrow and effect-based:

- layout sizing: width, height, margin, padding
- display/flex orientation where visible in boxes
- border and border radius where visible in screenshot/UI boxes
- background color where screenshot validation or visual health can observe it
- text style where UI dump/screenshot can observe it
- overflow and clipping where visible from boxes/screenshot

Computed style API is not required for first-round acceptance. If the runtime
cannot expose a property through UI dump or screenshot-level evidence, record it
as a coverage gap instead of asserting it.

## Artifact Contract

Each accepted run should produce:

- `report.json`
- `trace.jsonl`
- `screenshot.png`
- scenario-specific after screenshots when interaction or animation is tested
- `ui-dump.json`
- scenario-specific after UI dumps when interaction or animation is tested
- `ui-snapshot.json`
- scenario-specific after UI snapshots when interaction or animation is tested
- assertion output from `scripts/assert-ui.mjs`

## Acceptance Rules

A first HRT-020 slice is accepted only when:

- The test package is parallel to `src/packages/lynxtron-headless`.
- Fixtures are written using open Lynx element/CSS/JS interfaces from `lynx/`.
- Build command succeeds for the E2E package.
- Headless harness run succeeds with `backend: windowless-software`.
- At least one scenario proves tap-driven visible state change.
- At least one scenario covers a scroll/list behavior or records a precise
  current gap.
- At least one scenario covers a form element interaction or records a precise
  current gap.
- At least one scenario covers animation final state or records a precise
  current gap.
- Assertion script validates expected visible behavior from artifacts.
- Assertion script validates normalized snapshot behavior from
  `LynxSnapshot.capture`.
- Any skipped ability is recorded in
  `docs/headless-runtime/experiments/HRT-020-capability-gaps.md`.

## Workflow

1. Inventory interface files and choose a small representative API matrix.
2. Implement the parallel E2E package and fixtures.
3. Build the fixture bundle.
4. Run with `@lynx-js/lynxtron-headless`.
5. Assert artifact-backed behavior.
6. Record unsupported capabilities in the gap document.
7. PM review accepts only executable evidence, not screenshots alone.

## Open Risks

- The current harness may not support keyboard text input yet.
- The current UI dump may not expose enough detail for all CSS effects.
- Animation timing may require new wait primitives or frame sampling.
- Image rendering may hit known windowless texture gaps.
- `list` virtualization may need runtime-specific observation beyond first
  visible item count.

## First Slice Implementation Record

Implemented `src/packages/lynxtron-interface-e2e/` as the first standalone E2E
package parallel to `src/packages/lynxtron-headless`.

Covered in the first smoke fixture:

- Basic `view`/`text` rendering with nested boxes and visible text assertions.
- Tap interaction via `bindtap`/`catchtap`, asserted through visible text
  changes and harness tap report fields. The latest smoke covers a five-step
  CDP sequence: `Run interaction smoke`, `Nested child action`,
  `Catch child action`, `Scroll row 4`, and `Prime form state`.
- Nested interaction expectation: tapping `Nested child action` inside a parent
  `view` updates both child and parent visible state, proving nested hit target
  selection and `bindtap` bubbling for the stable path.
- Nested catch interaction expectation: tapping `Catch child action` updates the
  child visible state while the parent remains idle, proving the stable
  `catchtap` path for nested surfaces.
- `input` and `textarea` public element state path using controlled values,
  placeholder, `maxlength`, and `maxlines`; real keyboard insertion remains
  covered by HRT-020-G01.
- `scroll-view` public tag/attribute path using vertical orientation,
  `initial-scroll-to-index`, visible rows, and state-marked scroll content;
  imperative scroll/list method coverage remains follow-up work.
- Animation final state through CSS transition-backed state change and visible
  final text; intermediate frame assertions remain covered by HRT-020-G02.
- Small CSS effect gallery for border/radius, opacity, transform, transition,
  color, sizing, padding, and box assertions.

Validation command:

```text
cd src/packages/lynxtron-interface-e2e
node scripts/run-smoke.mjs
```

Latest observed result:

- Build succeeded and emitted `dist/lynx_open_interface_e2e.bundle`.
- Headless run succeeded with `backend: windowless-software`.
- `scripts/assert-ui.mjs` passed against visible texts, positive boxes,
  `report.json`, screenshot files, trace output, and normalized UI snapshots.
- Interaction assertion verified a five-step CDP tap sequence:
  `Run interaction smoke`, `Nested child action`, `Catch child action`,
  `Scroll row 4`, and `Prime form state`, all with `changed: true`.
- Scroll-view row assertion verified `Scroll row 4` after
  `LynxInput.scrollIntoView`, with `scrolled: true` and `scrollCount: 1`.
- Nested interaction assertion verified final `nested parent seen` and
  `nested child seen` text after tapping the nested child.
- Catch interaction assertion verified final `catch parent idle` and
  `catch child seen` text after tapping the catch child.
- Snapshot assertions passed for visible text, document text, action candidate
  positive box, visual health, and `LynxSnapshot.capture` protocol fields.
- Artifacts were written under
  `src/packages/lynxtron-interface-e2e/artifacts/smoke/`.

Observed bug-hunt result and fix:

- Adding `Scroll row 4` to the same CDP tap sequence found a current
  scroll-view clipping gap. The UI dump exposed the text and a target box, but
  the derived tap point was outside its nearest `scroll-view` clipping box and
  the runtime reported `INPUT_NO_VISUAL_CHANGE`.
- The harness now exposes `LynxInput.scrollIntoView` as a Lynx custom CDP
  method. Semantic `--tap-text` calls use it when the target is clipped by a
  scroll ancestor, then re-dump UI and tap the updated target.
- The latest smoke accepts the scroll-view row path for macOS Lynxtron
  headless. Broader page-scroll, wheel, drag, and mobile-provider behavior
  remain follow-up coverage.
