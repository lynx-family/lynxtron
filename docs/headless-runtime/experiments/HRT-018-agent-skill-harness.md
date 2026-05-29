# HRT-018: Agent Harness Skill Experiment

## Question

Can an agent with no prior project background use a small
`lynxtron-headless-harness` skill to create a complex Lynx page, build it, run
the headless harness, assert UI dump changes, and replay the interaction?

## Skill Under Test

`docs/headless-runtime/skills/lynxtron-headless-harness/SKILL.md`

The skill intentionally excludes record mode. It covers fixture creation,
bundle build, true-headless run, screenshot/UI dump artifacts, CDP tap by text,
dump assertions, and replay.

## Task Given To The Agent

Create fixture:

`src/packages/lynxtron-headless/fixtures/agent-decision-board/`

Bundle:

`headless_agent_decision.bundle`

Required initial texts:

- `Mode triage`
- `Selected Alpha`
- `Queue 7`
- `Risk medium`
- `Escalations 1`
- `Primary action pending`

Tap target:

- `Promote Beta`

Required after-tap texts:

- `Mode execute`
- `Selected Beta`
- `Queue 5`
- `Risk high`
- `Escalations 2`
- `Primary action armed`

Validation required from the agent:

- build fixture
- run `lynxtron-headless run` with `--tap-text "Promote Beta"`
- assert before/after UI dump texts
- run replay
- assert replay after UI dump texts

## Evaluation Criteria

The skill is useful if the agent can:

- discover and follow the local fixture structure without extra background
- avoid unsupported image/texture paths
- produce a realistic complex no-image Lynx page
- build a `.bundle`
- use the harness CLI correctly
- assert behavior from UI dump artifacts rather than app internals
- explain any blocker precisely if it fails

## Result

Passed.

The agent created:

- `src/packages/lynxtron-headless/fixtures/agent-decision-board/package.json`
- `src/packages/lynxtron-headless/fixtures/agent-decision-board/lynx.config.mjs`
- `src/packages/lynxtron-headless/fixtures/agent-decision-board/index.tsx`
- `src/packages/lynxtron-headless/fixtures/agent-decision-board/index.css`
- `src/packages/lynxtron-headless/fixtures/agent-decision-board/assert-ui-dump.mjs`
- `src/packages/lynxtron-headless/fixtures/agent-decision-board/dist/headless_agent_decision.bundle`

Independent review reran:

```sh
npm --prefix src/packages/lynxtron-headless/fixtures/agent-decision-board run build
```

Result:

- exit code `0`
- bundle generated: `dist/headless_agent_decision.bundle`
- bundle size: `103.9 kB`

```sh
node src/packages/lynxtron-headless/cli.js run \
  src/packages/lynxtron-headless/fixtures/agent-decision-board/dist/headless_agent_decision.bundle \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-agent-decision-review \
  --timeout 12000 \
  --width 390 \
  --height 844 \
  --dpr 3 \
  --tap-text "Promote Beta"
```

Result:

- exit code `0`
- `status: success`
- `backend: windowless-software`
- `input.tap.source: ui-dump`
- `input.tap.targetText: Promote Beta`
- `input.tap.protocol: cdp`
- `input.tap.changed: true`

```sh
node src/packages/lynxtron-headless/fixtures/agent-decision-board/assert-ui-dump.mjs \
  /tmp/lynxtron-agent-decision-review before-after
```

Result:

- before dump contains all required initial texts
- before dump contains exactly one `Promote Beta`
- after dump contains all required updated texts

```sh
node src/packages/lynxtron-headless/cli.js replay \
  /tmp/lynxtron-agent-decision-review/replay.json \
  --runtime out/Debug/lynxtron.app/Contents/MacOS/lynxtron \
  --artifact-dir /tmp/lynxtron-agent-decision-review-replay \
  --timeout 12000
```

Result:

- exit code `0`
- replay report has `status: success`
- replay report has `input.tap.changed: true`

```sh
node src/packages/lynxtron-headless/fixtures/agent-decision-board/assert-ui-dump.mjs \
  /tmp/lynxtron-agent-decision-review-replay after
```

Result:

- replay after dump contains all required updated texts

## Product Read

The skill is useful enough for a no-background agent to produce a meaningful
complex Lynx fixture and validate it through the harness. The agent did not
need project-specific explanation beyond the skill.

Observed skill improvement:

- Add an explicit warning that the first run can take several minutes and that
  agents should still return status if blocked. The agent succeeded but was
  silent long enough to require a status nudge.
