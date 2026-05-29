# HRT-015: Replay Recording

## Status

Done and accepted for the macOS MVP slice.

## Objective

Record enough structured metadata from a successful headless CDP run to replay
the same scenario later.

## Scope

- Write `replay.json` as a first-class artifact.
- Record source hash, runtime metadata, device profile, load options, CDP action
  params, semantic tap target, result summary, and artifact hashes.
- Add SDK replay entry point.
- Add CLI replay command.
- Support semantic replay by re-finding the tap target from `Lynx.dumpUITree`.
- Support exact replay by reusing recorded coordinates.

## Out Of Scope

- Multi-step arbitrary CDP script replay.
- Full locator model.
- Cross-machine artifact storage or bundle fetching.
- Visual diff.

## Acceptance

- A complex smoke run writes `replay.json`.
- `replay.json` has schema version, source hash, runtime/device/load metadata,
  recorded CDP actions, and artifact hashes.
- `lynxtron-headless replay <replay.json>` passes in semantic mode.
- `lynxtron-headless replay <replay.json> --mode exact` passes in exact mode.
- Both replay modes preserve the expected before/after UI dump assertions.

## Verification

```sh
npm --prefix src/packages/lynxtron-headless test
```

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/complex-app/dist/headless_complex.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-record \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --smoke complex
```

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-replay-semantic
```

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-headless-record/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-headless-replay-exact \
  --mode exact
```
