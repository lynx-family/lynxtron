#!/usr/bin/env node
'use strict';

const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');

// When run as a postinstall script, process.cwd() is the package's directory
// i.e., /path/to/project/node_modules/lynxtron-builder
const packagePath = process.cwd();
console.log(`[lynxtron-builder] Package path: ${packagePath}`);
console.log(`[lynxtron-builder] INIT_CWD: ${process.env.INIT_CWD}`);

// The project root is where the top-level package.json is.
// `npm` and `yarn` set `INIT_CWD` to the project root.
let projectRoot = process.env.INIT_CWD;
if (!projectRoot) {
  let candidate = path.resolve(packagePath, '../..');
  if (!fs.existsSync(path.join(candidate, 'node_modules'))) {
    candidate = path.resolve(packagePath, '../../..');
  }
  projectRoot = candidate;
}

console.log('[lynxtron-builder] Applying patches for the project...');
console.log(`[lynxtron-builder] Project root detected as: ${projectRoot}`);

try {
  // We need to execute patch-package in the context of the project root.
  // The `patch-package` command itself is available in the `PATH` during postinstall.
  
  // --patch-dir must be a relative path from the project root (where we run the command).
  const absolutePatchesDir = path.join(packagePath, 'patches');
  console.log(`[lynxtron-builder] Patches directory absolute path: ${absolutePatchesDir}`);
  const relativePatchesDir = path.relative(projectRoot, absolutePatchesDir);
  console.log(`[lynxtron-builder] Patches directory relative to project root: ${relativePatchesDir}`);

  const npxCommand = process.platform === 'win32' ? 'npx.cmd' : 'npx';
  const command = `${npxCommand} patch-package --patch-dir "${relativePatchesDir}"`;
  
  console.log(`[lynxtron-builder] Executing: ${command}`);
  
  execSync(command, { cwd: projectRoot, stdio: 'inherit' });
  
  console.log('[lynxtron-builder] Patches applied successfully.');
} catch (error) {
  console.error('[lynxtron-builder] Failed to apply patches.');
  console.error(error);
  process.exit(1);
}
