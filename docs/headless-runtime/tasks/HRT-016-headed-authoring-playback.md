# HRT-016: Headed Authoring Playback

## Status

Done and accepted for the macOS authoring/debug slice.

## Objective

Let a developer run or replay the CDP MVP flow with a visible host window and
slow-motion protocol steps for debugging.

## Scope

- Add CLI/SDK option `headed`.
- Add CLI/SDK option `slowMo`.
- Show a visible authoring host window while preserving the windowless/CDP
  control path.
- Delay key CDP actions when `slowMo` is set.
- Record authoring mode in `report.json`, `trace.jsonl`, and `replay.json`.

## Out Of Scope

- Manual human input recording.
- True native GUI renderer CDP dump/capture/input backing.
- Step/pause inspector UI.
- Overlay hit-target visualization.

## Acceptance

- `run --headed --slow-mo <ms>` exits with code `0`.
- `replay --headed --slow-mo <ms>` exits with code `0`.
- Reports include `authoring.headed: true`.
- Reports include `authoring.rendererMode: windowless-renderer-visible-host`.
- Traces include `authoring.headed-window`.
- Traces include `cdp.slow-mo` events.
- CDP UI dump before/after assertions still pass.

## Verification

```sh
npm --prefix src/packages/lynxtron-headless test
```

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-headed-replay \
  --headed \
  --slow-mo 50
```

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/complex-app/dist/headless_complex.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-headed-run \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --smoke complex \
  --headed \
  --slow-mo 10
```
