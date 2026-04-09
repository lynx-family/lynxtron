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

type EventCallback = { sendReply: (...args: any[]) => void };

describe('communication between node and lynx', () => {
  afterEach(closeAllWindows);

  it('jsb from lynx', async function () {
    this.timeout(30000);
    const w = new LynxWindow({
      width: 800,
      height: 600,
      title: 'Lynxtron JSB Demo',
    });

    const invokePromise = once(w as any, '-lynx-invoke') as Promise<
      [EventCallback, string, any]
    >;

    const messagePromise = once(w as any, '-lynx-message') as Promise<
      [string, any]
    >;

    const bundlePath = path.resolve(
      __dirname,
      './case/lynx-card/dist/bridging-lynx-node.lynx.bundle'
    );
    expect(fs.existsSync(bundlePath)).to.equal(true);
    const started = (w as any).loadURL('file://' + bundlePath);

    expect(started).to.equal(true);

    const [callback, methodName, params] = await Promise.race([
      invokePromise,
      setTimeout(10000).then(() => {
        throw new Error('Timed out waiting for -lynx-invoke');
      }),
    ]);

    expect(methodName).to.equal('onRender-test-event');
    expect(params.msg).to.equal('test-test');
    callback.sendReply({ methodName, params });

    const [messageMethod, messageParams] = await Promise.race([
      messagePromise,
      setTimeout(10000).then(() => {
        throw new Error('Timed out waiting for -lynx-message');
      }),
    ]);

    expect(messageMethod).to.equal('callback');
    expect(messageParams.from).to.deep.equal('-lynx-invoke-callback');
    expect(w.isDestroyed()).to.equal(false);
  });

  it('contextBridge', async function () {
    this.timeout(30000);
    const w = new LynxWindow({
      width: 800,
      height: 600,
      title: 'Lynxtron JSB Demo',
      nodeIntegration: {
        preload_paths: [
          path.resolve(
            __dirname,
            './case/lynx-card/src/contextbridge-lynx-node/preload.js'
          ),
        ],
      },
    });

    const messagePromise = once(w as any, '-lynx-message') as Promise<
      [string, any]
    >;

    const bundlePath = path.resolve(
      __dirname,
      './case/lynx-card/dist/contextbridge-lynx-node.lynx.bundle'
    );
    expect(fs.existsSync(bundlePath)).to.equal(true);
    const started = await (w as any).loadURL('file://' + bundlePath);

    expect(started).to.equal(true);

    await w.sendGlobalEvent('node_event', { msg: 'test-test' });

    const [messageMethod, messageParams] = await Promise.race([
      messagePromise,
      setTimeout(10000).then(() => {
        throw new Error('Timed out waiting for -lynx-message');
      }),
    ]);

    expect(messageMethod).to.equal('nodejs_event');
    expect(messageParams.from).to.deep.equal('contextBridge');
    expect(w.isDestroyed()).to.equal(false);
  });
});
