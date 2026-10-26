// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const { app } = require('lynxtron');
const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

app.whenReady().then(() => {
  const packageJson = JSON.parse(
    fs.readFileSync(path.join(__dirname, 'package.json'), 'utf8')
  );
  const result = {
    ready: app.isReady(),
    type: process.type,
    arch: process.arch,
    platform: process.platform,
    totalMemory: os.totalmem(),
    packageName: packageJson.name,
    digest: crypto.createHash('sha256')
      .update('lynxtron startup snapshot')
      .digest('hex'),
    bufferHex: Buffer.from('node').toString('hex'),
  };
  process.stdout.write(`NODE_SNAPSHOT_RESULT ${JSON.stringify(result)}\n`, () => {
    app.exit(0);
  });
}).catch((error) => {
  console.error(error);
  app.exit(1);
});
