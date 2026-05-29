#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import { join } from 'node:path';

const scenarios = {
  scenario1: {
    label: 'Scenario 1 Product Navigation',
    before: [
      'Lynx Market',
      'Daily Picks',
      'Open Nova Desk Lamp',
      'Recommend',
      'Profile',
    ],
    after: [
      'Product Detail',
      'Detail Nova Desk Lamp',
      '$48',
      'Inventory 24',
      'Add Nova To Cart',
      'Back To Recommendations',
    ],
  },
  scenario2: {
    label: 'Scenario 2 Profile Tab',
    before: ['Lynx Market', 'Recommend', 'Profile'],
    after: [
      'Profile Center',
      'Mina Chen',
      'Gold member',
      'Points 4820',
      'History',
      'Viewed Nova Desk Lamp',
      'Settings',
      'Notification Settings',
    ],
  },
};

const expectedProductNames = [
  'Nova Desk Lamp',
  'Orbit Knit Jacket',
  'Canvas Mini Tote',
  'Pulse Travel Speaker',
  'Bloom Ceramic Mug',
  'Zen Desk Mat',
  'Terra Pour Over Kit',
  'Metro Sling Pack',
  'Aero Laptop Stand',
  'Luma Braided Cable',
];

function usage() {
  console.error(
    'Usage: node assert-ui-dump.mjs <scenario1|scenario2|home-scroll> <artifact-dir>'
  );
  process.exit(5);
}

function collectTextsFromNode(node) {
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
  visit(node);
  return texts;
}

function collectTexts(dump) {
  if (Array.isArray(dump?.texts)) {
    return dump.texts;
  }
  return collectTextsFromNode(dump?.root);
}

function flattenNodes(dump) {
  const nodes = [];
  const visit = (node) => {
    if (!node || typeof node !== 'object') return;
    nodes.push(node);
    for (const child of node.children || []) {
      visit(child);
    }
  };
  visit(dump?.root);
  return nodes;
}

async function readJson(path) {
  return JSON.parse(await readFile(path, 'utf8'));
}

async function readTexts(path) {
  return collectTexts(await readJson(path));
}

function assertIncludes(label, texts, expected) {
  const missing = expected.filter((text) => !texts.includes(text));
  if (missing.length > 0) {
    throw new Error(
      `${label} missing: ${missing.join(', ')}\n${label} observed texts: ${texts.join(
        ' | '
      )}`
    );
  }
  console.log(`${label} contains: ${expected.join(', ')}`);
}

function assertHomeScrollContainer(dump) {
  const nodes = flattenNodes(dump);
  const scrollNodes = nodes.filter((node) =>
    String(node?.type || '')
      .toLowerCase()
      .includes('scroll')
  );
  const recommendationScroll = scrollNodes.find((node) => {
    const texts = collectTextsFromNode(node);
    return expectedProductNames.every((name) => texts.includes(name));
  });

  if (!recommendationScroll) {
    const observedTypes = [...new Set(nodes.map((node) => node?.type).filter(Boolean))];
    const observedScrollTypes = scrollNodes.map((node) => node.type);
    throw new Error(
      [
        'home missing scroll container with all recommendation products',
        `observed node types: ${observedTypes.join(', ')}`,
        `observed scroll-like types: ${observedScrollTypes.join(', ') || '(none)'}`,
        `home observed texts: ${collectTexts(dump).join(' | ')}`,
      ].join('\n')
    );
  }

  console.log(
    `home recommendation scroll ok: type=${recommendationScroll.type}, products=${expectedProductNames.length}`
  );
}

function assertReport(report) {
  const checks = [
    ['report.status', report?.status, 'success'],
    ['report.backend', report?.backend, 'windowless-software'],
    ['report.input.tap.protocol', report?.input?.tap?.protocol, 'cdp'],
    ['report.input.tap.changed', report?.input?.tap?.changed, true],
  ];
  const failures = checks
    .filter(([, actual, expected]) => actual !== expected)
    .map(([field, actual, expected]) => `${field} expected ${expected}, got ${actual}`);
  if (failures.length > 0) {
    throw new Error(failures.join('\n'));
  }
  console.log(
    `report ok: status=${report.status}, backend=${report.backend}, tap.protocol=${report.input.tap.protocol}, tap.changed=${report.input.tap.changed}`
  );
}

const [scenarioName, artifactDir] = process.argv.slice(2);
if ((!scenarios[scenarioName] && scenarioName !== 'home-scroll') || !artifactDir) {
  usage();
}

if (scenarioName === 'home-scroll') {
  const dump = await readJson(join(artifactDir, 'ui-dump.json'));
  assertHomeScrollContainer(dump);
  process.exit(0);
}

const scenario = scenarios[scenarioName];
const before = await readTexts(join(artifactDir, 'ui-dump.json'));
const after = await readTexts(join(artifactDir, 'ui-dump-after-tap.json'));
const report = await readJson(join(artifactDir, 'report.json'));

console.log(`Asserting ${scenario.label}`);
assertReport(report);
assertIncludes('before', before, scenario.before);
assertIncludes('after', after, scenario.after);
