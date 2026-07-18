// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const path = require('node:path');
const { execFileSync } = require('node:child_process');

const packageJson = process.env.npm_package_json;
if (!packageJson) {
  throw new Error('npm_package_json is required to install Git hooks');
}
const root = path.resolve(path.dirname(packageJson), '..');
execFileSync('git', ['-C', root, 'config', 'core.hooksPath', '.githooks'], {
  stdio: 'inherit',
});
