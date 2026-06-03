#!/usr/bin/env node

import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdir } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const packageRoot = dirname(dirname(fileURLToPath(import.meta.url)));
const workspaceRoot = resolve(packageRoot, '../../..');
const artifactDir = resolve(packageRoot, 'artifacts/smoke');
const bundle = resolve(packageRoot, 'dist/lynx_open_interface_e2e.bundle');
const headlessCli = resolve(packageRoot, '../lynxtron-headless/cli.js');
const runtimeCandidates = [
  process.env.LYNXTRON_HEADLESS_RUNTIME,
  resolve(workspaceRoot, 'out/Debug/lynxtron.app/Contents/MacOS/lynxtron'),
  resolve(workspaceRoot, 'out/Release/lynxtron.app/Contents/MacOS/lynxtron'),
  resolve(workspaceRoot, 'out/DebugSystemMalloc/lynxtron.app/Contents/MacOS/lynxtron'),
  resolve(workspaceRoot, 'out/ReleaseSkitySystemMalloc/lynxtron.app/Contents/MacOS/lynxtron'),
  resolve(packageRoot, '../lynxtron/dist/darwin/arm64/lynxtron.app/Contents/MacOS/lynxtron'),
  resolve(workspaceRoot, 'lynxtron/packages/lynxtron/dist/darwin/arm64/lynxtron.app/Contents/MacOS/lynxtron'),
].filter(Boolean);

function runtimeArg() {
  const explicitIndex = process.argv.indexOf('--runtime');
  if (explicitIndex !== -1 && process.argv[explicitIndex + 1]) {
    return process.argv[explicitIndex + 1];
  }
  return runtimeCandidates.find((candidate) => existsSync(candidate));
}

function run(command, args, options = {}) {
  return new Promise((resolvePromise, reject) => {
    const child = spawn(command, args, {
      cwd: options.cwd || packageRoot,
      stdio: 'inherit',
      env: {
        ...process.env,
        PATH: `${join(workspaceRoot, 'node_modules/.bin')}:${process.env.PATH || ''}`,
      },
    });
    child.on('error', reject);
    child.on('close', (code) => {
      if (code === 0) {
        resolvePromise();
      } else {
        reject(new Error(`${command} ${args.join(' ')} exited ${code}`));
      }
    });
  });
}

await mkdir(artifactDir, { recursive: true });

await run('npm', ['run', 'build']);
const runtime = runtimeArg();
const headlessArgs = [
  headlessCli,
  'run',
  bundle,
  '--artifact-dir',
  artifactDir,
  '--tap-text',
  'Run interaction smoke',
  '--tap-text',
  'Event matrix inner action',
  '--tap-text',
  'Nested child action',
  '--tap-text',
  'Catch child action',
  '--tap-text',
  'Scroll row 4',
  '--tap-text',
  'Reorder keyed items',
  '--tap-text',
  'Prime form state',
  '--drag-text',
  'Nested scroll row 4:0,-120',
  '--drag-text',
  'Selectable conflict text drag target:0,-80',
  '--width',
  '390',
  '--height',
  '844',
  '--timeout',
  '10000',
];
if (runtime) {
  headlessArgs.push('--runtime', runtime);
}
await run('node', headlessArgs);
await run('node', [resolve(packageRoot, 'scripts/assert-ui.mjs'), artifactDir]);

console.log(`artifacts: ${artifactDir}`);
