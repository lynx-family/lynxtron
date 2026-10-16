// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

const { app, LynxWindow } = require('lynxtron');
const assert = require('node:assert/strict');
const path = require('node:path');

const mode = process.argv[2];
// Keep the app alive when testing pages closed before the quit request.
app.on('window-all-closed', () => {});
app.on('quit', () => {
  assert.throws(() => new LynxWindow({ show: false }), /shutting down/);
});

async function createWindow(id) {
  const window = new LynxWindow({
    windowless: true,
    lynxPreference: { preload: path.join(__dirname, 'preload.js') },
  });
  const ready = new Promise((resolve) => window.once('-lynx-message', resolve));
  assert.equal(
    window.loadFile(
      path.resolve(
        __dirname,
        '../../../case/lynx-card/dist/runtime-shutdown.lynx.bundle'
      ),
      { globalProps: { windowId: id } }
    ),
    true
  );
  if (mode !== 'pending') await ready;
  return window;
}

app
  .whenReady()
  .then(async () => {
    if (mode === 'empty') return app.quit();
    const windows = [];
    for (let id = 0; id < 3; ++id) windows.push(await createWindow(id));
    if (mode === 'exit' || mode === 'pending') return app.exit(23);
    if (mode === 'closed') {
      windows.forEach((window) => window.destroy());
    } else if (mode === 'cancel') {
      app.once('will-quit', (event) => event.preventDefault());
      app.quit();
      // A canceled quit must not seal the runtime registry.
      (await createWindow(3)).destroy();
    }
    setImmediate(() => app.quit());
  })
  .catch((error) => {
    console.error(error);
    app.exit(1);
  });
