# HRT-013: CDP UI Dump Backed By Windowless Native Tree

## Status

Done and accepted for the macOS MVP slice.

## Objective

Provide `Lynx.dumpUITree` through the CDP control plane, backed by the real
windowless Lynx UI tree rather than direct public SDK access to `LynxWindow`.

## Scope

- Add internal native dump backing for the windowless renderer.
- Serialize node id, type, visibility, attributes, absolute viewport box, text,
  children, node count, text list, viewport, and backend.
- Bridge the native backing to the headless harness as an internal method only.
- Expose the durable operation as `Lynx.dumpUITree` over CDP.

## Out Of Scope

- Public TypeScript `LynxWindow.dumpUITree`.
- Full DOM/CSS inspector parity.
- Accessibility tree or React component tree.

## Acceptance

- `Lynx.dumpUITree` returns `available: true` for the headless windowless
  renderer.
- Text assertions can be made from the dump without reading JS app state.
- Boxes are viewport-absolute and can be used to derive input coordinates.

## Verification

- `ninja -C out/Debug lynxtron_app`
- Complex smoke command recorded in `docs/headless-runtime/product-plan.md`
- `ui-dump.json` and `ui-dump-after-tap.json` artifacts written by the harness
