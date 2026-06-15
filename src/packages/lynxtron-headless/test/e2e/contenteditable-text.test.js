import assert from 'node:assert/strict';
import test from 'node:test';
import { existsSync } from 'node:fs';
import { mkdtemp, readFile, stat } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';
import { runBundle } from '../../lib/index.js';

const testDir = dirname(fileURLToPath(import.meta.url));
const packageRoot = resolve(testDir, '../..');
const repoRoot = resolve(packageRoot, '../../..');
const fixtureRoot = resolve(packageRoot, 'fixtures/contenteditable-text');
const bundlePath = resolve(fixtureRoot, 'dist/headless_contenteditable_text.bundle');

function resolveRuntimeBinary() {
  const candidates = [
    process.env.LYNXTRON_HEADLESS_RUNTIME,
    resolve(repoRoot, 'out/Debug/lynxtron.app/Contents/MacOS/lynxtron'),
    resolve(repoRoot, 'packages/lynxtron/dist/darwin/arm64/lynxtron.app/Contents/MacOS/lynxtron'),
  ].filter(Boolean);
  return candidates.find((candidate) => existsSync(candidate));
}

function flattenTexts(node, output = []) {
  if (!node || typeof node !== 'object') {
    return output;
  }
  if (typeof node.text === 'string' && node.text.length > 0) {
    output.push(node.text);
  }
  for (const child of node.children || []) {
    flattenTexts(child, output);
  }
  return output;
}

async function assertFixtureUsesEventRefactor() {
  const bundle = await readFile(bundlePath);
  assert.ok(
    bundle.includes(Buffer.from('enableEventHandleRefactor')),
    'contenteditable fixture must enable event refactor'
  );
}

test('real headless contenteditable text accepts CDP insertText', async (t) => {
  const runtimeBinary = resolveRuntimeBinary();
  if (!runtimeBinary) {
    t.skip('Lynxtron runtime binary is unavailable');
    return;
  }
  if (!existsSync(bundlePath)) {
    t.skip('contenteditable text fixture bundle is not built');
    return;
  }
  await assertFixtureUsesEventRefactor();

  const dir = await mkdtemp(join(tmpdir(), 'lynxtron-headless-ce-text-e2e-'));
  const artifactDir = join(dir, 'artifacts');
  const result = await runBundle(bundlePath, {
    runtimeBinary,
    artifactDir,
    width: 760,
    height: 920,
    tapText: 'Editable text baseline',
    insertText: ' + e2e',
    timeoutMs: 30000,
  });

  assert.equal(result.ok, true, result.stderr);

  const report = JSON.parse(await readFile(result.artifacts.report, 'utf8'));
  assert.equal(report.status, 'success');
  assert.equal(report.input.tap.targetText, 'Editable text baseline');
  assert.equal(report.input.tap.changed, true);
  assert.equal(report.input.text.last.method, 'Input.insertText');
  assert.equal(report.input.text.last.accepted, true);
  assert.equal(report.input.text.last.changed, true);

  const dump = JSON.parse(await readFile(result.artifacts.uiDumpAfterTap, 'utf8'));
  const texts = flattenTexts(dump.root);
  assert.ok(
    texts.some(
      (text) =>
        text.includes('Editable text baseline') &&
        text.includes('place the caret') &&
        text.endsWith(' + e2e')
    ),
    `missing edited text; got ${texts.join(' | ')}`
  );
  assert.ok(
    texts.some(
      (text) =>
        text.includes('beforeinput') &&
        text.includes('insertText') &&
        text.includes('handled=yes')
    ),
    `missing frontend beforeinput handling log; got ${texts.join(' | ')}`
  );

  assert.ok((await stat(result.artifacts.screenshot)).size > 0);
  assert.ok((await stat(result.artifacts.tapScreenshot)).size > 0);
});

test('real headless contenteditable text keeps sequential frontend edits in order', async (t) => {
  const runtimeBinary = resolveRuntimeBinary();
  if (!runtimeBinary) {
    t.skip('Lynxtron runtime binary is unavailable');
    return;
  }
  if (!existsSync(bundlePath)) {
    t.skip('contenteditable text fixture bundle is not built');
    return;
  }
  await assertFixtureUsesEventRefactor();

  const dir = await mkdtemp(join(tmpdir(), 'lynxtron-headless-ce-text-pm-e2e-'));
  const artifactDir = join(dir, 'artifacts');
  const result = await runBundle(bundlePath, {
    runtimeBinary,
    artifactDir,
    width: 760,
    height: 920,
    tapText: 'Editable text baseline',
    insertTexts: [' + first', ' + second'],
    timeoutMs: 30000,
  });

  assert.equal(result.ok, true, result.stderr);

  const report = JSON.parse(await readFile(result.artifacts.report, 'utf8'));
  assert.equal(report.status, 'success');
  assert.equal(report.input.text.last.accepted, true);
  assert.equal(report.input.text.sequence.length, 2);
  assert.equal(report.input.text.sequence[0].changed, true);
  assert.equal(report.input.text.sequence[1].changed, true);

  const dump = JSON.parse(await readFile(result.artifacts.uiDumpAfterTap, 'utf8'));
  const texts = flattenTexts(dump.root);
  assert.ok(
    texts.some(
      (text) =>
        text.includes('Editable text baseline') &&
        text.includes('place the caret') &&
        text.endsWith(' + first + second')
    ),
    `sequential frontend edits landed out of order; got ${texts.join(' | ')}`
  );
  assert.ok(
    texts.some((text) => text.includes('undo=2') && text.includes('redo=0')),
    `missing frontend history depth; got ${texts.join(' | ')}`
  );
  assert.ok(
    texts.some((text) => text.includes('beforeinput') && text.includes('handled=yes')),
    `missing beforeinput log; got ${texts.join(' | ')}`
  );
});
