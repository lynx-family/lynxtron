#!/usr/bin/env node

import { access, readFile, stat } from 'node:fs/promises';
import { join } from 'node:path';

const [artifactDir = 'artifacts/smoke'] = process.argv.slice(2);

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

function collectTextBoxes(dump) {
  const boxes = [];
  const visit = (node) => {
    if (!node || typeof node !== 'object') return;
    if (typeof node.text === 'string' && node.text.length > 0 && node.box) {
      boxes.push({ text: node.text, box: node.box });
    }
    for (const child of node.children || []) {
      visit(child);
    }
  };
  visit(dump?.root);
  return boxes;
}

function snapshotTexts(snapshot) {
  return (snapshot.visibleTextRuns || []).map((run) => run.text).filter(Boolean);
}

function snapshotDocumentTexts(snapshot) {
  return (snapshot.documentTextRuns || snapshot.visibleTextRuns || [])
    .map((run) => run.text)
    .filter(Boolean);
}

function fail(message, details) {
  console.error(message);
  if (details) {
    console.error(details);
  }
  process.exitCode = 1;
}

function assertIncludes(label, texts, expected) {
  const missing = expected.filter((text) => !texts.includes(text));
  if (missing.length > 0) {
    fail(`${label} missing texts: ${missing.join(', ')}`, `${label} observed: ${texts.join(' | ')}`);
  } else {
    console.log(`${label} texts ok`);
  }
}

function assertNotIncludes(label, texts, unexpected) {
  const present = unexpected.filter((text) => texts.includes(text));
  if (present.length > 0) {
    fail(`${label} unexpected texts: ${present.join(', ')}`, `${label} observed: ${texts.join(' | ')}`);
  } else {
    console.log(`${label} absent texts ok`);
  }
}

function assertTextOrder(label, texts, expected) {
  let cursor = -1;
  const positions = [];
  for (const text of expected) {
    const index = texts.findIndex((candidate, candidateIndex) => candidateIndex > cursor && candidate === text);
    if (index === -1) {
      fail(`${label} order missing text: ${text}`, `${label} observed: ${texts.join(' | ')}`);
      return;
    }
    positions.push(index);
    cursor = index;
  }
  console.log(`${label} order ok: ${positions.join(' < ')}`);
}

function assertAnyIncludes(label, texts, expectedFragments) {
  const missing = expectedFragments.filter(
    (fragment) => !texts.some((text) => text.includes(fragment))
  );
  if (missing.length > 0) {
    fail(`${label} missing text fragments: ${missing.join(', ')}`, `${label} observed: ${texts.join(' | ')}`);
  } else {
    console.log(`${label} text fragments ok`);
  }
}

function assertPositiveBox(label, boxes, text) {
  const match = boxes.find((item) => item.text === text || item.text.includes(text));
  const box = match?.box;
  const ok =
    box &&
    Number.isFinite(box.width) &&
    Number.isFinite(box.height) &&
    box.width > 0 &&
    box.height > 0;
  if (!ok) {
    fail(`${label} missing positive box for "${text}"`);
  } else {
    console.log(`${label} box ok: ${text} ${box.width}x${box.height}`);
  }
}

async function readJson(path) {
  return JSON.parse(await readFile(path, 'utf8'));
}

async function assertFileNonEmpty(path, label) {
  await access(path);
  const info = await stat(path);
  if (info.size <= 0) {
    fail(`${label} is empty: ${path}`);
  } else {
    console.log(`${label} ok: ${info.size} bytes`);
  }
}

const beforeDump = await readJson(join(artifactDir, 'ui-dump.json'));
const afterDump = await readJson(join(artifactDir, 'ui-dump-after-tap.json'));
const beforeSnapshot = await readJson(join(artifactDir, 'ui-snapshot.json'));
const afterSnapshot = await readJson(join(artifactDir, 'ui-snapshot-after-tap.json'));
const report = await readJson(join(artifactDir, 'report.json'));

const beforeTexts = collectTexts(beforeDump);
const afterTexts = collectTexts(afterDump);
const beforeBoxes = collectTextBoxes(beforeDump);
const afterBoxes = collectTextBoxes(afterDump);

assertIncludes('before', beforeTexts, [
  'Lynx open interface E2E',
  'report: ready',
  'Run interaction smoke',
  'tap result idle',
  'Basic view text boxes',
  'view box stable',
  'text nested',
  'Interaction bug hunt lab',
  'Event matrix inner action',
  'event matrix result event matrix idle',
  'Nested event surface',
  'Nested child action',
  'nested parent idle',
  'nested child idle',
  'Catch child action',
  'catch parent idle',
  'catch child idle',
  'Form controls',
  'form programmatic state pending',
  'Scroll view',
  'scroll public state initial index 3',
  'Key item alpha',
  'Key item bravo',
  'Key item charlie',
  'Reorder keyed items',
  'keyed order alpha bravo charlie',
  'CSS effect gallery',
  'motion final idle',
]);

assertAnyIncludes('before', beforeTexts, [
  'report: tap count',
  'report: animation final',
  'report: form value',
  'report: scroll state',
  'report: nested state',
]);

assertIncludes('after', afterTexts, [
  'nested parent seen',
  'nested child seen',
  'event matrix result outer-capture>inner-bind>middle-bind>outer-bind',
  'catch parent idle',
  'catch child seen',
  'scroll public state row 4 tapped',
  'Key item delta',
  'keyed reorder charlie alpha delta',
  'tap result active',
  'form programmatic state alpha-42 line two',
  'motion final done',
]);

assertNotIncludes('after', afterTexts, [
  'Key item bravo',
  'keyed order alpha bravo charlie',
]);

assertTextOrder('after keyed items', afterTexts, [
  'Key item charlie',
  'Key item alpha',
  'Key item delta',
]);

assertAnyIncludes('after', afterTexts, [
  'report: tap count',
  'report: animation final',
  'alpha-42',
]);

assertPositiveBox('before', beforeBoxes, 'Run interaction smoke');
assertPositiveBox('before', beforeBoxes, 'view box stable');
assertPositiveBox('before', beforeBoxes, 'CSS effect gallery');
assertPositiveBox('after', afterBoxes, 'motion final done');

assertIncludes('snapshot visible before', snapshotTexts(beforeSnapshot), [
  'Lynx open interface E2E',
  'Run interaction smoke',
]);
assertIncludes('snapshot visible after', snapshotTexts(afterSnapshot), [
  'tap result active',
]);
assertIncludes('snapshot document before', snapshotDocumentTexts(beforeSnapshot), [
  'CSS effect gallery',
]);
assertIncludes('snapshot document after', snapshotDocumentTexts(afterSnapshot), [
  'nested parent seen',
  'nested child seen',
  'event matrix result outer-capture>inner-bind>middle-bind>outer-bind',
  'catch parent idle',
  'catch child seen',
  'scroll public state row 4 tapped',
  'keyed reorder charlie alpha delta',
  'form programmatic state alpha-42 line two',
  'motion final done',
]);

const runAction = (beforeSnapshot.actionCandidates || []).find((candidate) =>
  candidate.label?.includes('Run interaction smoke')
);
if (!runAction || !runAction.box || runAction.box.width <= 0 || runAction.box.height <= 0) {
  fail('snapshot missing positive action candidate for Run interaction smoke');
} else {
  console.log(`snapshot action ok: ${runAction.label} ${runAction.box.width}x${runAction.box.height}`);
}

if (beforeSnapshot.visualHealth?.blank || afterSnapshot.visualHealth?.blank) {
  fail('snapshot visual health reports blank page');
} else {
  console.log('snapshot visual health ok');
}

if (beforeSnapshot.protocol !== 'cdp' || beforeSnapshot.method !== 'LynxSnapshot.capture') {
  fail('snapshot protocol contract failed', JSON.stringify(beforeSnapshot.source || {}));
} else {
  console.log('snapshot protocol ok');
}

if (!['ok', 'success'].includes(report.status) || report.exitCode !== 0) {
  fail(`report status failed: status=${report.status} exitCode=${report.exitCode}`);
} else {
  console.log('report ok');
}

if (!report.input?.tap?.changed) {
  fail('report input tap did not record a visual change');
} else {
  console.log('tap visual change ok');
}

const tapSequence = report.input?.tap?.sequence || [];
if (tapSequence.length !== 7) {
  fail(`report tap sequence length failed: ${tapSequence.length}`);
} else if (
  tapSequence[0]?.targetText !== 'Run interaction smoke' ||
  tapSequence[1]?.targetText !== 'Event matrix inner action' ||
  tapSequence[2]?.targetText !== 'Nested child action' ||
  tapSequence[3]?.targetText !== 'Catch child action' ||
  tapSequence[4]?.targetText !== 'Scroll row 4' ||
  tapSequence[5]?.targetText !== 'Reorder keyed items' ||
  tapSequence[6]?.targetText !== 'Prime form state'
) {
  fail('report tap sequence target order failed', JSON.stringify(tapSequence));
} else if (!tapSequence.every((tap) => tap.protocol === 'cdp' && tap.changed)) {
  fail('report tap sequence protocol/change failed', JSON.stringify(tapSequence));
} else if (!tapSequence[4]?.scrolled || tapSequence[4]?.scrollCount <= 0) {
  fail('report tap sequence did not record scrollIntoView for Scroll row 4', JSON.stringify(tapSequence[4]));
} else {
  console.log('tap sequence ok');
}

await assertFileNonEmpty(join(artifactDir, 'screenshot.png'), 'screenshot');
await assertFileNonEmpty(join(artifactDir, 'screenshot-after-tap.png'), 'screenshot-after-tap');
await assertFileNonEmpty(join(artifactDir, 'trace.jsonl'), 'trace');
await assertFileNonEmpty(join(artifactDir, 'ui-snapshot.json'), 'ui-snapshot');
await assertFileNonEmpty(join(artifactDir, 'ui-snapshot-after-tap.json'), 'ui-snapshot-after-tap');

if (process.exitCode) {
  process.exit(process.exitCode);
}
