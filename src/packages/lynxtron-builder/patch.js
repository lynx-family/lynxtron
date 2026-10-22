#!/usr/bin/env node
'use strict';

const { execFileSync } = require('child_process');
const path = require('path');

// Only the private source workspace runs this script. Published packages
// receive their patched dependencies from prepare-bundled-deps.js instead.
const projectRoot = path.resolve(__dirname, '../..');
const patchesDir = path.relative(projectRoot, path.join(__dirname, 'patches'));

execFileSync(process.execPath, [
  require.resolve('patch-package/index.js'),
  '--patch-dir', patchesDir, '--error-on-fail',
], { cwd: projectRoot, stdio: 'inherit' });
