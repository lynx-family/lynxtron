# HRT-021 CDP Text Input

## Objective

Add cross-backend text input support through the harness CDP control plane.
This must not be a Lynxtron-only native API exposed to tests.

## Product Requirement

The public test contract is CDP-compatible input methods:

```text
Input.insertText
Input.dispatchKeyEvent
Input.clearText
```

If a backend cannot implement full keyboard events immediately, it may support
`Input.insertText` first and record the missing key/IME behavior as a follow-up.

## Backend Model

```text
harness CDP method
  -> backend provider bridge
      -> macOS Lynxtron focused element input
      -> Android explorer app input bridge
      -> iOS simulator/explorer input bridge
      -> non-Lynxtron host bridge
  -> visible UI state / event result
```

The test fixture must call only the CDP method or harness SDK wrapper. It must
not call a platform-private text setter.

## First Scope

- `Input.insertText({ text })`
- optional `Input.clearText()`
- focused `input` and `textarea`
- artifact-backed assertion through UI dump and report

Out of first scope:

- IME composition
- platform keyboard UI
- shortcut keys
- precise selection/caret manipulation

## Acceptance

- HRT-020 input/textarea fixture can focus an input, call CDP text input, and
  observe the value through UI dump or visible status text.
- `report.json` records the input method and target.
- `trace.jsonl` records CDP request/response and provider result.
- The same method name is suitable for mobile/explorer provider implementation.

## 2026-06-03 Harness Slice

Added harness-side `Input.insertText({ text })` plumbing and CLI/SDK
`--insert-text` support. The method records `accepted`, `errorCode`, protocol,
provider, and provider result in `report.json` under `input.text` and in
`trace.jsonl` as `input.text`.

Recorded replay now preserves `Input.insertText` actions. Semantic replay can
also map `manifest.input.text.sequence[].text` back to `--headless-insert-text`.

Current runtime blocker: Lynxtron headless exposes `dispatchHeadlessPointerEvent`
but no focused element text insertion provider. Until a provider such as
`dispatchHeadlessTextInput(text)` or an equivalent focused editable bridge is
available, `Input.insertText` fails explicitly with `INPUT_TEXT_UNAVAILABLE`.
