# HRT-022 CDP Animation Observation

## Objective

Add animation observation primitives through the CDP control plane so E2E tests
can verify animation-triggered UI without platform-specific hooks.

## Product Requirement

Expose protocol-level methods such as:

```text
LynxAnimation.waitForStableFrames
LynxAnimation.captureFrames
LynxAnimation.waitForAnimationEnd
```

The protocol should work even when the backend only has frame-level knowledge.
Animation timeline/state data can be added when a backend exposes it.

## Backend Model

```text
CDP animation method
  -> frame/screenshot sampler
  -> optional backend animation event bridge
  -> artifacts and trace
```

Mobile and non-Lynxtron hosts can implement the same methods by forwarding
frame events, screenshots, or animation lifecycle events from their explorer
app/runtime bridge.

## First Scope

- wait for two or more stable frames after an action
- capture before/after screenshots around animation trigger
- report frame count and timing window
- no millisecond-precise assertion

Out of first scope:

- exact frame-by-frame visual diff
- CSS animation timeline parity
- Web Animations API parity

## Acceptance

- HRT-020 animation case can trigger animation, wait through CDP, and assert
  final visible state with screenshot/frame evidence.
- `trace.jsonl` records animation wait start/end, frame counts, and timeout
  reason if failed.
- The protocol method can be implemented by mobile/explorer providers without
  changing test code.
