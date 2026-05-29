# HRT-012: Runtime-Local CDP Control Plane

## Status

Done and accepted for the macOS MVP slice.

## Objective

Expose a runtime-local CDP-compatible WebSocket endpoint from the headless
harness so SDK, CLI, future Playwright adapters, and future AI tools can use the
same control plane.

## Scope

- Implement a minimal CDP-style WebSocket server inside the headless harness.
- Implement a minimal Node CDP client for the SDK/CLI harness path.
- Support bundle/url load, lifecycle waits, screenshot, metrics, UI dump, and
  touch input through protocol methods.
- Emit protocol request/response traces.

## Out Of Scope

- Full Chrome CDP compatibility.
- Playwright adapter.
- MCP tool/server.
- Multi-session attach to arbitrary Lynxtron apps.

## Acceptance

- CLI smoke loads a real Lynx bundle through `Lynx.loadBundle`.
- Screenshot is captured through `Page.takeScreenshot`.
- Touch input is dispatched through `Input.dispatchTouchEvent`.
- `report.json` includes `protocol.cdp.wsEndpoint`.
- `trace.jsonl` includes CDP request and response events.

## Verification

- `npm --prefix src/packages/lynxtron-headless test`
- `ninja -C out/Debug lynxtron_app`
- Complex smoke command recorded in `docs/headless-runtime/product-plan.md`
