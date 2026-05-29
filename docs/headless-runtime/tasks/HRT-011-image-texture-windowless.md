# HRT-011: Image And Texture Support For Software Windowless

## Objective

Make image/texture-heavy Lynx bundles pass the same macOS true-headless smoke
path that the no-image fixture already passes.

## Problem

The accepted Phase 1a smoke uses Lynx windowless software rendering and passes
for `src/packages/lynxtron-headless/fixtures/smoke-app/dist/headless_smoke.bundle`.

The existing `default_app` bundle currently crashes in the software windowless
image path:

```text
lynx/clay/gfx/image/image_producer.cc:566
Check failed: unref_queue->GetContext()
```

This blocks accepting image-heavy bundles, visual fixture coverage, and broader
cloud/CI value.

## In Scope

- Investigate the Clay/skity image producer lifecycle when running with
  `HeadlessWindowlessRenderer`.
- Identify whether the software windowless path needs a context, unref queue,
  resource provider, or image fallback change.
- Add the smallest native fix that keeps the no-image smoke passing.
- Add or reuse one image-bearing smoke bundle.
- Run the standard Phase 1a smoke command against both no-image and image
  fixtures.

## Out Of Scope

- Linux support.
- Playwright adapter.
- MCP tool.
- JSON mock DSL.
- A broad image pipeline refactor unrelated to windowless rendering.

## File Ownership

Likely files:

- `lynx/clay/gfx/image/image_producer.cc`
- `lynx/clay/gfx/`
- `lynx/clay/ui/resource/`
- `src/shell/api/lynx_view/headless_windowless_renderer.*`
- `src/packages/lynxtron-headless/fixtures/`

Confirm exact ownership during investigation before editing additional paths.

## Acceptance

- The existing no-image acceptance smoke still exits `0`.
- An image-bearing bundle exits `0` with:
  - `backend: "windowless-software"`
  - `completionSignal: "on-first-screen"`
  - non-empty `screenshot.png`
  - non-empty `trace.jsonl`
- No user-visible native window is required.
- The failure at `image_producer.cc:566` is not reproducible for the accepted
  image fixture.

## Required Verification

```sh
ninja -C out/Debug lynxtron_app
npm --prefix src/packages/lynxtron-headless test
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/smoke-app/dist/headless_smoke.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-acceptance \
  --timeout 10000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --tap 195,422
```

Add the matching image-fixture smoke command once the fixture path is chosen.

## Stop Conditions

- The fix requires replacing the current Clay/skity backend rather than
  adapting the windowless path.
- The image fixture only passes by falling back to visible or hidden native
  window rendering.
- The fix regresses the accepted no-image headless smoke.
