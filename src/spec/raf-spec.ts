// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LynxWindow } from 'lynxtron';
import { expect } from 'chai';
import { once } from 'node:events';
import * as path from 'node:path';

import { closeAllWindows } from './lib/window-helpers';

describe('requestAnimationFrame', () => {
  afterEach(closeAllWindows);

  it('invokes the callback in a Lynx page', async () => {
    const window = new LynxWindow({ show: false, windowless: true });
    const callback = once(window, '-lynx-message');

    expect(
      window.loadFile(
        path.resolve(__dirname, 'case/lynx-card/dist/raf.lynx.bundle')
      )
    ).to.equal(true);

    const [methodName] = await callback;
    expect(methodName).to.equal('raf-callback');
  });
});
