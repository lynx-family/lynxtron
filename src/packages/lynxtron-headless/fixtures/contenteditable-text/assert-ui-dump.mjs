import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { join, resolve } from 'node:path';

const artifactDir = resolve(process.argv[2] || 'artifacts');
const initialText = 'Initial editable text';
const insertedText = ' + harness';
const expectedText = `${initialText}${insertedText}`;
const mode = process.argv[3] || 'insert';

async function readJson(path) {
  return JSON.parse(await readFile(path, 'utf8'));
}

function dumpTexts(dump) {
  if (Array.isArray(dump?.texts)) return dump.texts;
  const texts = [];
  const visit = (node) => {
    if (!node || typeof node !== 'object') return;
    if (typeof node.text === 'string' && node.text.length > 0) {
      texts.push(node.text);
    }
    for (const child of node.children || []) visit(child);
  };
  visit(dump?.root);
  return texts;
}

const report = await readJson(join(artifactDir, 'report.json'));

if (report?.error?.code === 'INPUT_TEXT_UNAVAILABLE') {
  const textResult = report?.input?.text?.last || report?.input?.text?.sequence?.at?.(-1);
  assert.equal(textResult?.method, 'Input.insertText');
  assert.equal(textResult?.accepted, false);
  assert.equal(textResult?.errorCode, 'INPUT_TEXT_UNAVAILABLE');
  console.log('BLOCKED: Input.insertText provider is unavailable in the current runtime.');
  process.exit(0);
}

assert.equal(report?.status, 'success');
assert.equal(report?.input?.text?.last?.method, 'Input.insertText');
assert.equal(report?.input?.text?.last?.accepted, true);
assert.equal(report?.input?.text?.last?.changed, true);

if (mode === 'backspace') {
  assert.equal(report?.input?.key?.last?.method, 'Input.dispatchKeyEvent');
  assert.equal(report?.input?.key?.last?.key, 'Backspace');
  assert.equal(report?.input?.key?.last?.accepted, true);
  assert.equal(report?.input?.key?.last?.changed, true);

  const uiDumpAfterTap = await readJson(join(artifactDir, 'ui-dump-after-tap.json'));
  const texts = dumpTexts(uiDumpAfterTap);
  assert.ok(texts.includes(initialText), `missing restored text: ${initialText}`);
  assert.ok(!texts.includes(`${initialText}😀`), 'emoji insert survived Backspace');
  console.log('PASS: contenteditable Backspace removed inserted emoji without corrupting text');
  process.exit(0);
}

const uiDumpAfterTap = await readJson(join(artifactDir, 'ui-dump-after-tap.json'));
const texts = dumpTexts(uiDumpAfterTap);
const editedText = texts.find(
  (text) => text.includes(insertedText) && text.replace(insertedText, '') === initialText
);
assert.ok(editedText, `missing edited text containing ${insertedText}`);

console.log(`PASS: contenteditable text accepted ${insertedText} and rendered ${editedText}`);
