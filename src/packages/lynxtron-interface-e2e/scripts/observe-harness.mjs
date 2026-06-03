#!/usr/bin/env node

import { existsSync } from 'node:fs';
import { readFile, stat } from 'node:fs/promises';
import { join, resolve } from 'node:path';

const artifactDir = resolve(process.argv[2] || 'artifacts/smoke');

async function readJsonIfExists(path) {
  if (!existsSync(path)) {
    return null;
  }
  return JSON.parse(await readFile(path, 'utf8'));
}

async function sizeIfExists(path) {
  if (!existsSync(path)) {
    return null;
  }
  return (await stat(path)).size;
}

function collectTextsFromDump(dump) {
  if (!dump) {
    return [];
  }
  if (Array.isArray(dump.texts)) {
    return dump.texts.filter(Boolean);
  }
  const texts = [];
  const visit = (node) => {
    if (!node || typeof node !== 'object') {
      return;
    }
    if (typeof node.text === 'string' && node.text.length > 0) {
      texts.push(node.text);
    }
    for (const child of node.children || []) {
      visit(child);
    }
  };
  visit(dump.root);
  return texts;
}

function collectTextsFromSnapshot(snapshot) {
  const runs = snapshot?.visibleTextRuns || snapshot?.documentTextRuns || [];
  return runs.map((run) => run.text).filter(Boolean);
}

function summarizeActions(report) {
  const tap = report?.input?.tap;
  const taps = tap?.tapSequence || tap?.sequence || (tap?.targetText ? [tap] : []);
  const drags = tap?.dragSequence || [];
  return { taps, drags };
}

function traceTailLines(traceText, limit = 16) {
  if (!traceText) {
    return [];
  }
  return traceText
    .trim()
    .split('\n')
    .filter(Boolean)
    .slice(-limit)
    .map((line) => {
      try {
        const event = JSON.parse(line);
        return [
          event.ts,
          event.type,
          event.method,
          event.code,
          event.message,
          event.targetText || event.text,
        ].filter(Boolean).join(' | ');
      } catch {
        return line;
      }
    });
}

function printSection(title) {
  console.log(`\n## ${title}`);
}

function printList(items, emptyText = '(none)') {
  if (!items || items.length === 0) {
    console.log(emptyText);
    return;
  }
  for (const item of items) {
    console.log(`- ${item}`);
  }
}

const reportPath = join(artifactDir, 'report.json');
const tracePath = join(artifactDir, 'trace.jsonl');
const beforeDumpPath = join(artifactDir, 'ui-dump.json');
const afterDumpPath = join(artifactDir, 'ui-dump-after-tap.json');
const beforeSnapshotPath = join(artifactDir, 'ui-snapshot.json');
const afterSnapshotPath = join(artifactDir, 'ui-snapshot-after-tap.json');
const screenshotPath = join(artifactDir, 'screenshot.png');
const afterScreenshotPath = join(artifactDir, 'screenshot-after-tap.png');
const replayPath = join(artifactDir, 'replay.json');

const [
  report,
  beforeDump,
  afterDump,
  beforeSnapshot,
  afterSnapshot,
  traceText,
  screenshotSize,
  afterScreenshotSize,
  replaySize,
] = await Promise.all([
  readJsonIfExists(reportPath),
  readJsonIfExists(beforeDumpPath),
  readJsonIfExists(afterDumpPath),
  readJsonIfExists(beforeSnapshotPath),
  readJsonIfExists(afterSnapshotPath),
  existsSync(tracePath) ? readFile(tracePath, 'utf8') : null,
  sizeIfExists(screenshotPath),
  sizeIfExists(afterScreenshotPath),
  sizeIfExists(replayPath),
]);

const beforeTexts = collectTextsFromSnapshot(beforeSnapshot);
const afterTexts = collectTextsFromSnapshot(afterSnapshot);
const beforeDumpTexts = collectTextsFromDump(beforeDump);
const afterDumpTexts = collectTextsFromDump(afterDump);
const { taps, drags } = summarizeActions(report);

console.log(`artifact: ${artifactDir}`);

printSection('Report');
if (!report) {
  console.log('report.json: missing');
} else {
  console.log(`status: ${report.status}`);
  console.log(`exitCode: ${report.exitCode}`);
  console.log(`backend: ${report.backend || report.headless?.backend || '(unknown)'}`);
  console.log(`viewport: ${report.device?.viewport?.width}x${report.device?.viewport?.height}`);
  console.log(`dpr: ${report.device?.dpr}`);
  console.log(`framesPresented: ${report.headless?.framesPresented}`);
  console.log(`rendererTasks: ${report.headless?.rendererTasksRun}/${report.headless?.rendererTasksPosted}`);
  if (report.error) {
    console.log(`error: ${report.error.code} ${report.error.message}`);
  }
}

printSection('Artifacts');
printList([
  `screenshot.png: ${screenshotSize ?? 'missing'} bytes`,
  `screenshot-after-tap.png: ${afterScreenshotSize ?? 'missing'} bytes`,
  `ui-dump.json: ${beforeDump ? beforeDumpTexts.length : 'missing'} texts`,
  `ui-dump-after-tap.json: ${afterDump ? afterDumpTexts.length : 'missing'} texts`,
  `ui-snapshot.json: ${beforeSnapshot ? beforeTexts.length : 'missing'} visible texts`,
  `ui-snapshot-after-tap.json: ${afterSnapshot ? afterTexts.length : 'missing'} visible texts`,
  `trace.jsonl: ${traceText ? traceText.split('\n').filter(Boolean).length : 'missing'} events`,
  `replay.json: ${replaySize ?? 'missing'} bytes`,
]);

printSection('Tap Sequence');
printList(taps.map((tap, index) => {
  const label = tap.targetText || tap.text || `(tap ${index + 1})`;
  return [
    `${index + 1}. ${label}`,
    `source=${tap.source}`,
    `changed=${tap.changed}`,
    `screenshotChanged=${tap.screenshotChanged}`,
    `uiDumpChanged=${tap.uiDumpChanged}`,
    `scrolled=${tap.scrolled}`,
    tap.scrollCount !== undefined ? `scrollCount=${tap.scrollCount}` : null,
    tap.protocol ? `protocol=${tap.protocol}` : null,
  ].filter(Boolean).join(' ');
}));

printSection('Drag Sequence');
printList(drags.map((drag, index) => {
  const label = drag.targetText || drag.text || `(drag ${index + 1})`;
  return [
    `${index + 1}. ${label}`,
    `dx=${drag.dx}`,
    `dy=${drag.dy}`,
    `changed=${drag.changed}`,
    `screenshotChanged=${drag.screenshotChanged}`,
    `uiDumpChanged=${drag.uiDumpChanged}`,
    `scrolled=${drag.scrolled}`,
    drag.scrollCount !== undefined ? `scrollCount=${drag.scrollCount}` : null,
    drag.protocol ? `protocol=${drag.protocol}` : null,
  ].filter(Boolean).join(' ');
}));

printSection('Visible Text Before');
printList(beforeTexts.slice(0, 80));

printSection('Visible Text After');
printList(afterTexts.slice(0, 80));

printSection('Trace Tail');
printList(traceTailLines(traceText), '(trace missing)');
