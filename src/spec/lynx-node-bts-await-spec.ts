// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LynxWindow } from 'lynxtron';

import { expect } from 'chai';
import { once } from 'node:events';
import * as fs from 'node:fs';
import * as path from 'node:path';
import { setTimeout } from 'node:timers/promises';

import { closeAllWindows } from './lib/window-helpers';

describe('lynx BTS await with contextBridge', () => {
  afterEach(closeAllWindows);

  it('async/await works in lynx BTS and node preload', async function () {
    this.timeout(30000);
    const w = new LynxWindow({
      width: 800,
      height: 600,
      title: 'Lynxtron BTS Await Test',
      nodeIntegration: {
        preload_paths: [
          path.resolve(
            __dirname,
            './case/lynx-card/src/lynx-node-bts-await/preload.js'
          ),
        ],
      },
    });

    const messagePromise = once(w as any, '-lynx-message') as Promise<
      [string, any]
    >;

    const bundlePath = path.resolve(
      __dirname,
      './case/lynx-card/dist/lynx-node-bts-await.lynx.bundle'
    );
    if (!fs.existsSync(bundlePath)) {
      this.skip();
      return;
    }
    const started = await w.loadFile(bundlePath);

    expect(started).to.equal(true);

    await w.sendGlobalEvent('node_event', { msg: 'test-await' });

    const [messageMethod, messageParams] = await Promise.race([
      messagePromise,
      setTimeout(10000).then(() => {
        throw new Error('Timed out waiting for -lynx-message');
      }),
    ]);

    expect(messageMethod).to.equal('nodejs_event');
    expect(messageParams.from).to.equal('contextBridge');
    expect(messageParams.message).to.equal(
      JSON.stringify({ msg: 'test-await' })
    );
    expect(w.isDestroyed()).to.equal(false);
  });
});
