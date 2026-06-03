# HRT-020 Capability Gaps

This document records Lynx open-interface E2E coverage gaps found while testing
generic elements, JavaScript interface behavior, component interaction,
animation, and CSS effects through the headless harness.

Rules:

- Record only observed or source-backed gaps.
- Do not mark an ability as unsupported only because no test has been written.
- If a scenario is skipped, include the missing harness/runtime capability and
  the narrowest next step.
- Keep the gap tied to the public interface under `lynx/`, not an internal
  implementation detail.

## Gap Table

| ID | Area | Public interface source | Missing or blocked capability | Impact on E2E | Proposed next step | Status |
| --- | --- | --- | --- | --- | --- | --- |
| HRT-020-G01 | Keyboard/input | `lynx/js_libraries/types/types/common/element/input.d.ts`, `lynx/js_libraries/types/types/common/element/textarea.d.ts` | Current harness CLI/CDP surface exposes `Input.dispatchTouchEvent`, but no generic key/text input method was found. | First slice can tap/focus and assert visible state changes, but cannot yet type arbitrary text into `input`/`textarea` through a stable harness API. | HRT-021: add protocol-facing `Input.insertText`/keyboard bridge so macOS Lynxtron, mobile explorer apps, and non-Lynxtron hosts can implement the same method. | Observed harness gap |
| HRT-020-G02 | Animation frame observation | `lynx/tools/css_generator/index.d.ts`, element style interfaces | Current harness exposes `Lynx.waitForFrame` and `Lynx.waitForFrameAfter`, but no dedicated animation lifecycle wait, intermediate frame sampler, or animation-state query was found. | First slice can assert final visual/state effect only; deterministic intermediate animation assertions should be skipped. | HRT-022: add CDP animation/frame observation methods with provider bridge support. | Observed harness gap |
| HRT-020-G03 | CSS computed/effect observation | `lynx/tools/css_generator/index.d.ts` | Current acceptance path is screenshot/UI dump/box based; no complete computed style API is available in the harness. | CSS tests should validate observable effects only; unsupported computed-style assertions must be skipped. | HRT-023/HRT-024: add `LynxSnapshot.capture` and selected CDP computed style subset. | Observed harness gap |
| HRT-020-G04 | Image rendering | TBD | TBD | TBD | TBD | Pending investigation |
| HRT-020-G05 | List virtualization | TBD | TBD | TBD | TBD | Pending investigation |
| HRT-020-G06 | Scroll ancestor clipping / scroll into view | `common/element/scroll-view.d.ts`, `common/element/common.d.ts` | UI dump can expose scroll-view children outside the container clipping box. Direct semantic tap can target clipped child coordinates and fail with `INPUT_NO_VISUAL_CHANGE`. The first failing case was `Scroll row 4`; after adding `LynxInput.scrollIntoView`, the smoke scrolls the nearest `scroll-view` through CDP touch move events and taps the updated target. | The fixture now proves scroll-view child clipping recovery on macOS Lynxtron headless. It still does not prove generic page scroll, wheel input, drag API shape, or mobile provider behavior. | Extend HRT-025 with page-scroll, wheel/drag, and mobile explorer provider cases behind the same protocol-facing API family. | Initial fixed; follow-up coverage remains |

## Notes

- CSS coverage can lag behind JavaScript, interaction, animation, and component
  behavior coverage for the first slice.
- When an effect cannot be observed through UI dump or screenshot, prefer
  recording the observation gap over asserting internal state.
- The first `src/packages/lynxtron-interface-e2e` smoke covers controlled
  form state and final animation state, but intentionally does not claim real
  keyboard text insertion, computed style validation, or deterministic
  intermediate animation frame sampling.
