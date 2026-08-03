// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be
// found in the LICENSE file in the root directory of this source tree.

import { LynxWindow } from 'lynxtron';
import { expect } from 'chai';
import * as fs from 'node:fs';
import { once } from 'node:events';
import * as os from 'node:os';
import * as path from 'node:path';
import { setTimeout } from 'node:timers/promises';
import { pathToFileURL } from 'node:url';

import { closeAllWindows } from './lib/window-helpers';

const { LynxTemplateBundle } = require('lynxtron') as {
  LynxTemplateBundle?: new (buffer: Buffer | ArrayBufferView) => {
    isValid(): boolean;
    getErrorMessage(): string;
  };
};

const bundlePath = path.resolve(
  __dirname,
  './case/lynx-card/dist/bridging-lynx-node.lynx.bundle'
);
const getBundleBuffer = () => {
  if (!fs.existsSync(bundlePath)) {
    return null;
  }
  return fs.readFileSync(bundlePath);
};

const createWindow = (title: string) =>
  new LynxWindow({
    width: 800,
    height: 600,
    title,
  });

const loadAndWait = async (
  window: LynxWindow,
  load: () => void,
  successEvent = 'ready-to-show'
) => {
  const loaded = once(window, successEvent);
  const failed = once(window, '--lynx-error').then(([, code, message]) => {
    throw new Error(`Lynx load failed (${String(code)}): ${String(message)}`);
  });

  load();
  await Promise.race([loaded, failed]);
};

describe('LynxWindow template APIs', () => {
  afterEach(closeAllWindows);

  it('exposes loadFile on LynxWindow', function () {
    expect(typeof (LynxWindow.prototype as any).loadFile).to.equal('function');
  });

  it('exposes loadURL on LynxWindow', function () {
    expect(typeof (LynxWindow.prototype as any).loadURL).to.equal('function');
  });

  it('exposes loadBundle on LynxWindow', function () {
    expect(typeof (LynxWindow.prototype as any).loadBundle).to.equal(
      'function'
    );
  });

  it('loadFile(path, { data, globalProps }) accepts a file path source', async function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }

    const w = createWindow('LynxWindow loadFile(path)');

    await loadAndWait(w, () =>
      (w as any).loadFile(bundlePath, {
        data: { foo: 'bar' },
        globalProps: { ver: 1 },
      })
    );
    expect(w.isDestroyed()).to.equal(false);
  });

  it('does not crash when collecting an unanswered Lynx bridge event', async function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }

    await (async () => {
      const w = createWindow('LynxWindow unanswered bridge event');
      const invokePromise = new Promise<{
        methodName: string;
        params: any;
        hasSendReply: boolean;
      }>((resolve) => {
        (w as any).once(
          '-lynx-invoke',
          (callback: any, methodName: string, params: any) => {
            resolve({
              methodName,
              params,
              hasSendReply: typeof callback.sendReply === 'function',
            });
          }
        );
      });

      await loadAndWait(w, () =>
        (w as any).loadFile(bundlePath, {
          data: { foo: 'bar' },
          globalProps: { ver: 1 },
        })
      );

      const { methodName, params, hasSendReply } = await invokePromise;
      expect(methodName).to.equal('onRender-test-event');
      expect(params.msg).to.equal('test-test');
      expect(hasSendReply).to.equal(true);

      // Intentionally leave the bridge event unanswered. Releasing the window
      // and the local callback reference makes its wrapper eligible for GC.
      await closeAllWindows();
    })();
    await setTimeout();

    const v8Util = process._linkedBinding('lynxtron_binding_v8_util');
    v8Util.requestGarbageCollectionForTesting();

    await setTimeout();
  });

  it('emits ready-to-show when template loading completes', async function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }

    const w = createWindow('LynxWindow ready-to-show event');

    await loadAndWait(
      w,
      () => (w as any).loadFile(bundlePath),
      'ready-to-show'
    );
    expect(w.isDestroyed()).to.equal(false);
  });

  it('loadURL(url, { data, globalProps }) accepts a file URL source', async function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }

    const w = createWindow('LynxWindow loadURL(file URL)');

    await loadAndWait(w, () =>
      (w as any).loadURL(pathToFileURL(bundlePath).href, {
        data: { foo: 'bar' },
        globalProps: { ver: 1 },
      })
    );
    expect(w.isDestroyed()).to.equal(false);
  });

  it('loadURL(url) decodes percent-escaped file URLs', async function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }

    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'lynxtron-bundle-'));
    const copiedBundlePath = path.join(
      tempDir,
      'bundle with space.lynx.bundle'
    );
    fs.copyFileSync(bundlePath, copiedBundlePath);

    const w = createWindow('LynxWindow loadURL(percent-escaped file URL)');

    try {
      await loadAndWait(w, () =>
        (w as any).loadURL(pathToFileURL(copiedBundlePath).href)
      );
      expect(w.isDestroyed()).to.equal(false);
    } finally {
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  });

  it('loadBundle(templateBundle, { data, globalProps }) accepts a pre-decoded bundle', async function () {
    if (typeof LynxTemplateBundle !== 'function') {
      this.skip();
    }
    const buf = getBundleBuffer();
    if (!buf) {
      this.skip();
    }

    const templateBundle = new LynxTemplateBundle(buf);
    if (!templateBundle.isValid()) {
      this.skip();
    }

    const w = createWindow('LynxWindow loadBundle(templateBundle)');

    await loadAndWait(w, () =>
      (w as any).loadBundle(templateBundle, {
        data: { foo: 'bar' },
        globalProps: { ver: 1 },
      })
    );
    expect(w.isDestroyed()).to.equal(false);
  });

  it('loadTemplate is absent when the new APIs are present', function () {
    const proto: any = LynxWindow.prototype;
    expect(proto.loadTemplate).to.be.oneOf([undefined, null]);
    expect(proto.loadFile).to.be.a('function');
    expect(proto.loadURL).to.be.a('function');
    expect(proto.loadBundle).to.be.a('function');
  });
});
