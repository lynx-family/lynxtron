// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');

setImmediate(() => {
  const result = {
    type: typeof process.type,
    arch: process.arch,
    platform: process.platform,
    nodeVersion: process.versions.node,
    lynxtronVersion: process.versions.lynxtron,
    totalMemory: os.totalmem(),
    fixtureLoaded: fs.statSync(__filename).isFile(),
    digest: crypto
      .createHash('sha256')
      .update('lynxtron startup snapshot')
      .digest('hex'),
  };
  console.log(`NODE_MODE_SNAPSHOT_RESULT ${JSON.stringify(result)}`);
});
