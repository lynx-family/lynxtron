// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be
// found in the LICENSE file in the root directory of this source tree.

import { LynxTemplateData, LynxUpdateMeta, LynxWindow } from 'lynxtron';
import { expect } from 'chai';
import * as fs from 'node:fs';
import { once } from 'node:events';
import * as os from 'node:os';
import * as path from 'node:path';
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

  it('exposes updateMetaData on LynxWindow', function () {
    expect(typeof (LynxWindow.prototype as any).updateMetaData).to.equal(
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

  it('updateMetaData(meta) accepts updateData/globalProps', function () {
    const w = new LynxWindow({
      width: 400,
      height: 300,
      title: 'LynxWindow updateMetaData(meta)',
      show: false,
    });

    const meta = new LynxUpdateMeta({
      updateData: new LynxTemplateData({ foo: 'bar' }),
      globalProps: new LynxTemplateData({ ver: 1 }),
    });
    expect(() => (w as any).updateMetaData(meta)).to.not.throw();
  });

  it('loadTemplate is absent when the new APIs are present', function () {
    const proto: any = LynxWindow.prototype;
    expect(proto.loadTemplate).to.be.oneOf([undefined, null]);
    expect(proto.loadFile).to.be.a('function');
    expect(proto.loadURL).to.be.a('function');
    expect(proto.loadBundle).to.be.a('function');
  });
});
