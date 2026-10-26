// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const { execFileSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const { SRC_DIR, getOutDir } = require('../../../../../script/lib/utils');

module.exports = () => {
  const headers = path.join(
    SRC_DIR,
    'out',
    getOutDir(),
    'include/third_party/weak-node-api/headers'
  );
  fs.mkdirSync(path.join(__dirname, 'build'), { recursive: true });
  execFileSync(
    'c++',
    [
      '-std=c++17',
      '-shared',
      '-fPIC',
      '-pthread',
      ...(process.platform === 'darwin'
        ? ['-undefined', 'dynamic_lookup']
        : []),
      `-I${headers}`,
      path.join(__dirname, 'preload.cc'),
      '-o',
      path.join(__dirname, 'build/preload.node'),
    ],
    { stdio: 'inherit' }
  );
};
