# HRT-017: CDP Recorder Monitor

## Objective

Expose input observation and recording through a provider-neutral CDP domain so
desktop and future mobile providers can share one replay manifest format.

## Scope

In scope:

- Add `LynxRecorder.*` CDP methods and events.
- Add macOS Lynxtron `NSWindow` observed-input provider.
- Add `lynxtron-headless record`.
- Add recorded-action replay mode.
- Preserve the existing true-headless CDP smoke behavior.

Out of scope:

- Mobile provider implementation.
- Full Chrome CDP compatibility.
- Playwright adapter or MCP tool layer.
- Visual diff.

## Accepted API

Methods:

- `LynxRecorder.start`
- `LynxRecorder.stop`
- `LynxRecorder.getEvents`
- `LynxRecorder.dispatchObservedInputEvent`

Events:

- `LynxRecorder.recordingStarted`
- `LynxRecorder.inputEvent`
- `LynxRecorder.actionRecorded`
- `LynxRecorder.uiSnapshot`
- `LynxRecorder.recordingStopped`
- `LynxRecorder.recordingError`

## Verification

- `npm --prefix src/packages/lynxtron-headless test`
- `ninja -C out/Debug lynxtron_app`
- complex CDP smoke at `/tmp/lynxtron-headless-cdp-monitor-smoke`
- CDP-provider injected record at `/tmp/lynxtron-headless-record-injected`
- recorded-action replay at `/tmp/lynxtron-headless-recorded-replay`

## Status

Accepted for the generic CDP recorder layer, recorded replay, and macOS provider
compile path.

Real human-click validation for the macOS provider is left as a manual desktop
check because automated OS-level input injection can depend on local
Accessibility permissions.
