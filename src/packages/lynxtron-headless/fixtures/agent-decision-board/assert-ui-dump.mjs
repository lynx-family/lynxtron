#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import { join } from 'node:path';

const beforeTexts = [
  'Mode triage',
  'Selected Alpha',
  'Queue 7',
  'Risk medium',
  'Escalations 1',
  'Primary action pending',
];

const afterTexts = [
  'Mode execute',
  'Selected Beta',
  'Queue 5',
  'Risk high',
  'Escalations 2',
  'Primary action armed',
];

function collectTexts(dump) {
  if (Array.isArray(dump?.texts)) {
    return dump.texts;
  }
  const texts = [];
  const visit = (node) => {
    if (!node || typeof node !== 'object') return;
    if (typeof node.text === 'string' && node.text.length > 0) {
      texts.push(node.text);
    }
    for (const child of node.children || []) {
      visit(child);
    }
  };
  visit(dump?.root);
  return texts;
}

async function readTexts(path) {
  const dump = JSON.parse(await readFile(path, 'utf8'));
  return collectTexts(dump);
}

function assertIncludes(label, texts, expected) {
  const missing = expected.filter((text) => !texts.includes(text));
  if (missing.length > 0) {
    console.error(`${label} missing: ${missing.join(', ')}`);
    console.error(`${label} observed texts: ${texts.join(' | ')}`);
    process.exitCode = 1;
    return;
  }
  console.log(`${label} ok: ${expected.join(', ')}`);
}

function assertCount(label, texts, expectedText, expectedCount) {
  const count = texts.filter((text) => text === expectedText).length;
  if (count !== expectedCount) {
    console.error(`${label} expected ${expectedCount} "${expectedText}" text, found ${count}`);
    console.error(`${label} observed texts: ${texts.join(' | ')}`);
    process.exitCode = 1;
    return;
  }
  console.log(`${label} ok: ${expectedText} count ${expectedCount}`);
}

const [artifactDir, mode = 'before-after'] = process.argv.slice(2);
if (!artifactDir) {
  console.error('Usage: node assert-ui-dump.mjs <artifact-dir> [before-after|after]');
  process.exit(5);
}

if (mode === 'before-after') {
  const before = await readTexts(join(artifactDir, 'ui-dump.json'));
  const after = await readTexts(join(artifactDir, 'ui-dump-after-tap.json'));
  assertIncludes('before', before, beforeTexts);
  assertCount('before', before, 'Promote Beta', 1);
  assertIncludes('after', after, afterTexts);
} else if (mode === 'after') {
  const after = await readTexts(join(artifactDir, 'ui-dump-after-tap.json'));
  assertIncludes('after', after, afterTexts);
} else {
  console.error(`Unknown assertion mode: ${mode}`);
  process.exit(5);
}
