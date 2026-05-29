import assert from 'node:assert/strict';
import test from 'node:test';
import { mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { createHarnessArgs, runBundle, runReplay } from '../lib/index.js';

test('createHarnessArgs maps bundle and artifact paths', () => {
  const { args, artifacts } = createHarnessArgs('/tmp/app.lynx.bundle', {
    artifactDir: '/tmp/out',
    width: 320,
    height: 640,
    dpr: 2,
    timeoutMs: 1234,
    tap: { x: 10, y: 20 },
    headed: true,
    slowMo: 50,
  });

  assert.equal(args[1], '--headless-bundle=/tmp/app.lynx.bundle');
  assert.ok(args.includes('--headless-width=320'));
  assert.ok(args.includes('--headless-height=640'));
  assert.ok(args.includes('--headless-dpr=2'));
  assert.ok(args.includes('--headless-timeout=1234'));
  assert.ok(args.includes('--headless-tap=10,20'));
  assert.ok(args.includes('--headless-headed=true'));
  assert.ok(args.includes('--headless-slow-mo=50'));
  assert.ok(args.includes('--headless-replay=/tmp/out/replay.json'));
  assert.ok(args.includes('--headless-tap-screenshot=/tmp/out/screenshot-after-tap.png'));
  assert.ok(args.includes('--headless-ui-dump=/tmp/out/ui-dump.json'));
  assert.ok(args.includes('--headless-ui-dump-after-tap=/tmp/out/ui-dump-after-tap.json'));
  assert.equal(artifacts.report, '/tmp/out/report.json');
  assert.equal(artifacts.tapScreenshot, '/tmp/out/screenshot-after-tap.png');
  assert.equal(artifacts.uiDump, '/tmp/out/ui-dump.json');
  assert.equal(artifacts.uiDumpAfterTap, '/tmp/out/ui-dump-after-tap.json');
  assert.equal(artifacts.replay, '/tmp/out/replay.json');
});

test('createHarnessArgs maps CDP recorder options', () => {
  const { args } = createHarnessArgs('/tmp/app.lynx.bundle', {
    artifactDir: '/tmp/out',
    record: true,
    recordDurationMs: 2500,
    allowEmptyRecording: true,
    replayScript: '/tmp/out/replay.json',
  });

  assert.ok(args.includes('--headless-record=true'));
  assert.ok(args.includes('--headless-record-duration=2500'));
  assert.ok(args.includes('--headless-allow-empty-recording=true'));
  assert.ok(args.includes('--headless-replay-script=/tmp/out/replay.json'));
});

test('runBundle launches runtime and returns artifact contract', async () => {
  const dir = await mkdtemp(join(tmpdir(), 'lynxtron-headless-'));
  const bundle = join(dir, 'main.lynx.bundle');
  const artifactDir = join(dir, 'artifacts');
  const mockRuntime = join(dir, 'mock-runtime.mjs');
  await mkdir(artifactDir, { recursive: true });
  await writeFile(bundle, 'fixture');
  await writeFile(
    mockRuntime,
    `#!/usr/bin/env node
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname } from 'node:path';
const args = process.argv.slice(2);
const get = (prefix) => args.find((arg) => arg.startsWith(prefix))?.slice(prefix.length);
const report = get('--headless-report=');
const trace = get('--headless-trace=');
const screenshot = get('--headless-screenshot=');
await mkdir(dirname(report), { recursive: true });
await writeFile(report, JSON.stringify({ status: 'success', args }));
await writeFile(trace, JSON.stringify({ type: 'mock' }) + '\\n');
await writeFile(screenshot, Buffer.from([137,80,78,71]));
`
  );

  const result = await runBundle(bundle, {
    runtimeBinary: process.execPath,
    harnessPath: mockRuntime,
    artifactDir,
  });

  assert.equal(result.ok, true);
  assert.equal(result.exitCode, 0);
  assert.equal(result.artifacts.artifactDir, artifactDir);
  assert.ok(result.args[0].endsWith('mock-runtime.mjs'));
});

test('runReplay maps semantic manifest to harness args', async () => {
  const dir = await mkdtemp(join(tmpdir(), 'lynxtron-headless-replay-'));
  const bundle = join(dir, 'main.lynx.bundle');
  const artifactDir = join(dir, 'replay-artifacts');
  const manifest = join(dir, 'replay.json');
  const mockRuntime = join(dir, 'mock-runtime.mjs');
  await mkdir(artifactDir, { recursive: true });
  await writeFile(bundle, 'fixture');
  await writeFile(
    manifest,
    JSON.stringify({
      kind: 'lynxtron-headless-replay',
      schemaVersion: 1,
      source: { type: 'bundle', value: bundle },
      runtime: { binary: process.execPath },
      authoring: { headed: true, slowMoMs: 99 },
      device: { viewport: { width: 390, height: 844 }, dpr: 3 },
      load: { timeoutMs: 12000, smoke: 'complex' },
      semantic: { tapText: 'Upgrade plan' },
      actions: [
        {
          method: 'Input.dispatchTouchEvent',
          params: { type: 'touchStart', touchPoints: [{ x: 195, y: 611 }] },
        },
      ],
    })
  );
  await writeFile(
    mockRuntime,
    `#!/usr/bin/env node
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname } from 'node:path';
const args = process.argv.slice(2);
const get = (prefix) => args.find((arg) => arg.startsWith(prefix))?.slice(prefix.length);
const report = get('--headless-report=');
const trace = get('--headless-trace=');
const screenshot = get('--headless-screenshot=');
await mkdir(dirname(report), { recursive: true });
await writeFile(report, JSON.stringify({ status: 'success', args }));
await writeFile(trace, JSON.stringify({ type: 'mock' }) + '\\n');
await writeFile(screenshot, Buffer.from([137,80,78,71]));
`
  );

  const result = await runReplay(manifest, {
    runtimeBinary: process.execPath,
    harnessPath: mockRuntime,
    artifactDir,
  });

  assert.equal(result.ok, true);
  assert.ok(result.args.includes('--headless-width=390'));
  assert.ok(result.args.includes('--headless-height=844'));
  assert.ok(result.args.includes('--headless-dpr=3'));
  assert.ok(result.args.includes('--headless-timeout=12000'));
  assert.ok(result.args.includes('--headless-smoke=complex'));
  assert.ok(result.args.includes('--headless-tap-text=Upgrade plan'));
  assert.ok(!result.args.includes('--headless-headed=true'));
  assert.ok(!result.args.includes('--headless-slow-mo=99'));
});

test('runReplay maps recorded manifest to replay script', async () => {
  const dir = await mkdtemp(join(tmpdir(), 'lynxtron-headless-recorded-replay-'));
  const bundle = join(dir, 'main.lynx.bundle');
  const artifactDir = join(dir, 'recorded-replay-artifacts');
  const manifest = join(dir, 'replay.json');
  const mockRuntime = join(dir, 'mock-runtime.mjs');
  await mkdir(artifactDir, { recursive: true });
  await writeFile(bundle, 'fixture');
  await writeFile(
    manifest,
    JSON.stringify({
      kind: 'lynxtron-headless-replay',
      schemaVersion: 1,
      replay: { defaultMode: 'recorded' },
      source: { type: 'bundle', value: bundle },
      runtime: { binary: process.execPath },
      device: { viewport: { width: 390, height: 844 }, dpr: 3 },
      load: { timeoutMs: 12000 },
      actions: [
        {
          method: 'Input.dispatchTouchEvent',
          params: { type: 'touchStart', touchPoints: [{ x: 10, y: 20 }] },
        },
        {
          method: 'Input.dispatchTouchEvent',
          params: { type: 'touchEnd', touchPoints: [{ x: 10, y: 20 }] },
        },
      ],
    })
  );
  await writeFile(
    mockRuntime,
    `#!/usr/bin/env node
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname } from 'node:path';
const args = process.argv.slice(2);
const get = (prefix) => args.find((arg) => arg.startsWith(prefix))?.slice(prefix.length);
const report = get('--headless-report=');
const trace = get('--headless-trace=');
const screenshot = get('--headless-screenshot=');
await mkdir(dirname(report), { recursive: true });
await writeFile(report, JSON.stringify({ status: 'success', args }));
await writeFile(trace, JSON.stringify({ type: 'mock' }) + '\\n');
await writeFile(screenshot, Buffer.from([137,80,78,71]));
`
  );

  const result = await runReplay(manifest, {
    runtimeBinary: process.execPath,
    harnessPath: mockRuntime,
    artifactDir,
  });

  assert.equal(result.ok, true);
  assert.ok(result.args.includes(`--headless-replay-script=${manifest}`));
  assert.ok(!result.args.some((arg) => arg.startsWith('--headless-tap=')));
  assert.ok(!result.args.some((arg) => arg.startsWith('--headless-tap-text=')));
});
