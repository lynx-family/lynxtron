// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be
// found in the LICENSE file in the root directory of this source tree.

import { LynxTemplateData, LynxUpdateMeta, LynxWindow } from 'lynxtron';
import { expect } from 'chai';
import * as fs from 'node:fs';
import { once } from 'node:events';
import * as path from 'node:path';

import { closeAllWindows } from './lib/window-helpers';

const bundlePath = path.resolve(
  __dirname,
  './case/lynx-card/dist/metadata-update.lynx.bundle'
);

type MetadataUpdateState = {
  theme: string;
  preserved: string;
  updated: string;
  initialFlag: string;
  replacementFlag: string;
};

const createWindow = (title: string) =>
  new LynxWindow({
    width: 800,
    height: 600,
    title,
  });

const waitForMetadataUpdateState = (
  window: LynxWindow,
  expected: MetadataUpdateState
): Promise<void> =>
  new Promise((resolve, reject) => {
    const received: unknown[] = [];
    const timeout = globalThis.setTimeout(() => {
      window.removeListener('-lynx-invoke', onInvoke);
      reject(
        new Error(
          `Timed out waiting for metadata update state: ${JSON.stringify(
            expected
          )}; received: ${JSON.stringify(received)}`
        )
      );
    }, 3000);

    const onInvoke = (
      callback: { sendReply?: (result: unknown) => void },
      methodName: string,
      params: unknown
    ) => {
      if (methodName !== 'metadata-update-state') {
        return;
      }

      received.push(params);
      callback.sendReply?.({});
      if (JSON.stringify(params) === JSON.stringify(expected)) {
        clearTimeout(timeout);
        window.removeListener('-lynx-invoke', onInvoke);
        resolve();
      }
    };

    window.on('-lynx-invoke', onInvoke);
  });

const loadAndWait = async (window: LynxWindow, load: () => void) => {
  const loaded = once(window, 'ready-to-show');
  const failed = once(window, '--lynx-error').then(([, code, message]) => {
    throw new Error(`Lynx load failed (${String(code)}): ${String(message)}`);
  });

  load();
  await Promise.race([loaded, failed]);
};

describe('LynxWindow metadata APIs', () => {
  afterEach(closeAllWindows);

  it('exposes updateMetaData on LynxWindow', function () {
    expect(typeof (LynxWindow.prototype as any).updateMetaData).to.equal(
      'function'
    );
  });

  it('updateMetaData(meta) accepts either updateData or globalProps', function () {
    const window = new LynxWindow({
      width: 400,
      height: 300,
      title: 'LynxWindow updateMetaData(meta)',
      show: false,
    });

    for (const meta of [
      new LynxUpdateMeta({
        updateData: new LynxTemplateData({ foo: 'bar' }),
      }),
      new LynxUpdateMeta({
        globalProps: new LynxTemplateData({ ver: 1 }),
      }),
      new LynxUpdateMeta({
        updateData: new LynxTemplateData({ foo: 'bar' }),
        globalProps: new LynxTemplateData({ ver: 1 }),
      }),
    ]) {
      expect(() => (window as any).updateMetaData(meta)).to.not.throw();
    }
  });

  it('updates metadata fields and replaces nested global props', async function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }

    const window = createWindow('LynxWindow updateMetaData');
    const initialState = waitForMetadataUpdateState(window, {
      theme: 'light',
      preserved: 'template-data',
      updated: '',
      initialFlag: 'initial-flag',
      replacementFlag: '',
    });
    await loadAndWait(window, () =>
      (window as any).loadFile(bundlePath, {
        data: { preserved: 'template-data' },
        globalProps: {
          appTheme: 'light',
          flags: { initial: 'initial-flag' },
        },
      })
    );
    await initialState;

    const dataOnlyState = waitForMetadataUpdateState(window, {
      theme: 'light',
      preserved: 'template-data',
      updated: 'data-only',
      initialFlag: 'initial-flag',
      replacementFlag: '',
    });
    expect(
      (window as any).updateMetaData(
        new LynxUpdateMeta({
          updateData: new LynxTemplateData({ updated: 'data-only' }),
        })
      )
    ).to.equal(true);
    await dataOnlyState;

    const propsOnlyState = waitForMetadataUpdateState(window, {
      theme: 'dark',
      preserved: 'template-data',
      updated: 'data-only',
      initialFlag: '',
      replacementFlag: 'props-only',
    });
    expect(
      (window as any).updateMetaData(
        new LynxUpdateMeta({
          globalProps: new LynxTemplateData({
            appTheme: 'dark',
            flags: { replacement: 'props-only' },
          }),
        })
      )
    ).to.equal(true);
    await propsOnlyState;

    const bothState = waitForMetadataUpdateState(window, {
      theme: 'contrast',
      preserved: 'template-data',
      updated: 'both',
      initialFlag: '',
      replacementFlag: 'both',
    });
    expect(
      (window as any).updateMetaData(
        new LynxUpdateMeta({
          updateData: new LynxTemplateData({ updated: 'both' }),
          globalProps: new LynxTemplateData({
            appTheme: 'contrast',
            flags: { replacement: 'both' },
          }),
        })
      )
    ).to.equal(true);
    await bothState;
  });

  it('setGlobalProps updates global props without replacing template data', async function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }

    const window = createWindow('LynxWindow setGlobalProps');
    const initialState = waitForMetadataUpdateState(window, {
      theme: 'light',
      preserved: 'template-data',
      updated: '',
      initialFlag: '',
      replacementFlag: '',
    });
    await loadAndWait(window, () =>
      (window as any).loadFile(bundlePath, {
        data: { preserved: 'template-data' },
        globalProps: { appTheme: 'light' },
      })
    );
    await initialState;

    const updatedState = waitForMetadataUpdateState(window, {
      theme: 'dark',
      preserved: 'template-data',
      updated: '',
      initialFlag: '',
      replacementFlag: '',
    });
    expect((window as any).setGlobalProps({ appTheme: 'dark' })).to.equal(true);
    await updatedState;
  });
});
