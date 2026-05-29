# HRT-014: Complex CDP Smoke

## Status

Done and accepted for the macOS MVP slice.

## Objective

Prove the MVP chain on a non-trivial Lynx page: screenshot, UI dump, UI
operation, after-operation UI dump, and behavioral assertions, all through CDP.

## Scope

- Add a checked-in complex no-image Lynx fixture.
- Run the fixture in true headless mode.
- Capture a screenshot through CDP.
- Dump the UI tree through CDP.
- Derive the tap target from the UI dump.
- Dispatch input through CDP.
- Dump the UI tree again and assert expected changed text state.

## Out Of Scope

- Image-heavy bundle support.
- JS NativeModules mock loading.
- Visual diff.
- External Playwright/MCP adapters.

## Acceptance

- Process exits with code `0`.
- Report status is `success`.
- Before dump contains `Selected Basic`, `Cart total $42`, `Status idle`, and
  `Items 3`.
- After dump contains `Selected Pro`, `Cart total $57`, `Status selected`, and
  `Items 4`.
- `input.tap.source` is `ui-dump`.
- `input.tap.protocol` is `cdp`.
- Screenshot changes after input.

## Verification

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/complex-app/dist/headless_complex.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-complex \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --smoke complex
```
