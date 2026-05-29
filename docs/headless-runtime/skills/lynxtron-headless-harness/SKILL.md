---
name: lynxtron-headless-harness
description: Build or validate Lynx React fixtures with the local lynxtron-headless harness, including bundle build, true-headless screenshot/UI dump, CDP tap, replay, and artifact assertions.
---

# Lynxtron Headless Harness

Use this skill when asked to create or validate a Lynx page with the local
`@lynx-js/lynxtron-headless` harness.

## Ground Rules

- Do not use record mode.
- Prefer a no-image fixture. Current accepted headless smoke coverage is for
  deterministic no-image Lynx bundles.
- Exercise real Lynx UI behavior: visible text state before and after a tap.
- Observe and operate through the harness/CDP path, not by reading app state.

## Repo Paths

Assume commands run from the repo root:

- CLI: `src/packages/lynxtron-headless/cli.js`
- Runtime: `out/Debug/lynxtron.app/Contents/MacOS/lynxtron`
- Fixtures: `src/packages/lynxtron-headless/fixtures/`

If the runtime is missing, build it first:

```sh
ninja -C out/Debug lynxtron_app
```

## Fixture Pattern

Create a fixture directory under
`src/packages/lynxtron-headless/fixtures/<fixture-name>/` with:

- `package.json`
- `lynx.config.mjs`
- `index.tsx`
- `index.css`

Use the existing `complex-app` fixture as the template. The bundle output should
be named distinctly, for example `headless_agent_dashboard.bundle`.

Lynx React basics:

```tsx
import { root, useState } from '@lynx-js/react';
import './index.css';

function App() {
  const [selected, setSelected] = useState(false);
  return (
    <view>
      <text>{selected ? 'Status selected' : 'Status idle'}</text>
      <view bindtap={() => setSelected(true)}>
        <text>Choose plan</text>
      </view>
    </view>
  );
}

root.render(<App />);
```

Prefer `bindtap` for the target interaction. Keep target text unique so the
harness can tap by `--tap-text`.

## Build

```sh
npm --prefix src/packages/lynxtron-headless/fixtures/<fixture-name> build
```

The expected bundle path is:

```text
src/packages/lynxtron-headless/fixtures/<fixture-name>/dist/<bundle-name>.bundle
```

## Harness Run

```sh
rm -rf /tmp/<artifact-dir>
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/<fixture-name>/dist/<bundle-name>.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/<artifact-dir> \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --tap-text "<unique target text>"
```

Required artifacts:

- `report.json`
- `trace.jsonl`
- `screenshot.png`
- `screenshot-after-tap.png`
- `ui-dump.json`
- `ui-dump-after-tap.json`
- `replay.json`

## Assertions

After the harness run, read `report.json`, `ui-dump.json`, and
`ui-dump-after-tap.json`.

The run is acceptable only if:

- process exit code is `0`
- `report.status` is `success`
- `report.backend` is `windowless-software`
- `report.input.tap.protocol` is `cdp`
- `report.input.tap.changed` is `true`
- before UI dump contains the expected initial texts
- after UI dump contains the expected updated texts

Use a small Node check for text assertions:

```sh
node - <<'NODE'
const fs = require('fs');
const before = JSON.parse(fs.readFileSync('/tmp/<artifact-dir>/ui-dump.json', 'utf8')).texts || [];
const after = JSON.parse(fs.readFileSync('/tmp/<artifact-dir>/ui-dump-after-tap.json', 'utf8')).texts || [];
const needBefore = ['Status idle'];
const needAfter = ['Status selected'];
for (const text of needBefore) {
  if (!before.includes(text)) throw new Error(`before missing ${text}`);
}
for (const text of needAfter) {
  if (!after.includes(text)) throw new Error(`after missing ${text}`);
}
NODE
```

## Replay Check

If asked to prove reproducibility, run:

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/<artifact-dir>/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/<artifact-dir>-replay \
  --timeout 12000
```

Then assert the replay report is successful and the after UI dump contains the
same updated texts.

## Delivery Note

Report:

- fixture files created or changed
- build command and result
- harness command and artifact directory
- key `report.json` fields
- before/after text assertions
- replay result if run
- any blocker with exact command output
