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

describe('LynxSetModule', () => {
  afterEach(closeAllWindows);

  it('allows keyboardstatuschanged listener registration', async function () {
    this.timeout(15000);

    const bundlePath = path.resolve(
      __dirname,
      './case/lynx-card/dist/lynx-set-module.lynx.bundle'
    );
    expect(fs.existsSync(bundlePath)).to.equal(true);

    const window = new LynxWindow({
      width: 800,
      height: 600,
      title: 'LynxSetModule compatibility',
    });
    const message = once(window as any, '-lynx-message') as Promise<
      [string, { registered: boolean }]
    >;
    const loadError = once(window as any, '--lynx-error').then(
      ([, code, errorMessage]) => {
        throw new Error(
          `Lynx load failed (${String(code)}): ${String(errorMessage)}`
        );
      }
    );

    expect(window.loadFile(bundlePath)).to.equal(true);

    const [method, params] = await Promise.race([
      message,
      loadError,
      setTimeout(10000).then(() => {
        throw new Error('Timed out waiting for LynxSetModule demo result');
      }),
    ]);

    expect(method).to.equal('lynx-set-module-ready');
    expect(params.registered).to.equal(true);
  });
});
