// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { expect } from 'chai';

import type { Notification as NotificationType } from '../packages/lynxtron/apis/api/notification';

const { Notification } = require('lynxtron') as {
  Notification: typeof NotificationType;
};

describe('Notification module', () => {
  it('exposes isSupported as a static method', () => {
    expect(Notification.isSupported).to.be.a('function');
    expect(Notification.isSupported()).to.equal(
      process.platform === 'darwin' || process.platform === 'win32'
    );
  });

  it('is exposed as a named ESM package export', async () => {
    const importModule = new Function(
      'specifier',
      'return import(specifier);'
    ) as (specifier: string) => Promise<typeof import('@lynx-js/lynxtron')>;
    const packageExports = await importModule('@lynx-js/lynxtron');

    expect(packageExports.Notification).to.equal(Notification);
    expect(packageExports.Notification.isSupported()).to.be.a('boolean');
  });
});
