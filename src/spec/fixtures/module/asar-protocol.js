// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const { app, protocol, LynxWindow } = require('lynxtron');
const assert = require('node:assert/strict');
const fs = require('original-fs');
const { pathToFileURL } = require('node:url');
const { loadAndWait } = require('../../lib/lynx-window-helpers');

const [templatePath, bundlePath, interceptAssets] = process.argv.slice(2);

app.whenReady().then(async () => {
  const templateUrl = pathToFileURL(templatePath).href;
  const templateBytes = fs.readFileSync(bundlePath);
  const requests = [];
  let rewrites = 0;

  protocol.setRequestRewriter((request) => {
    if (request.url === templateUrl) {
      rewrites++;
    }
    return request.url;
  });
  protocol.handle('file', (request) => {
    if (request.url !== templateUrl) {
      return false;
    }
    requests.push(request.resourceType);
    if (interceptAssets === 'true') {
      return {
        url: request.url,
        statusCode: request.resourceType === 'Template' ? 1 : 0,
        data: request.resourceType === 'Template' ? Buffer.alloc(0) : templateBytes,
      };
    }
    return false;
  });

  const window = new LynxWindow({ width: 800, height: 600, show: false });
  await loadAndWait(window, () => {
    assert.equal(window.loadFile(templatePath), true);
  });

  assert.equal(requests[0], 'Template');
  if (interceptAssets === 'true') {
    assert.equal(requests.length, 2);
    assert.notEqual(requests[1], 'Template');
  }
  assert.equal(rewrites, requests.length);
}).then(() => {
  process.exit(0);
}).catch((error) => {
  console.error(error);
  process.exit(1);
});
