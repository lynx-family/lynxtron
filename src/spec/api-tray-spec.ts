// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { Menu, Tray, nativeImage } from 'lynxtron';

import { expect } from 'chai';
import * as path from 'node:path';

import { ifdescribe } from './lib/spec-helpers';

const isSupportedPlatform =
  process.platform === 'darwin' || process.platform === 'win32';

ifdescribe(isSupportedPlatform)('tray module', () => {
  const fixturesPath = path.join(__dirname, 'fixtures', 'assets');
  const iconPath =
    process.platform === 'darwin'
      ? path.join(fixturesPath, 'logo_Template.png')
      : path.join(fixturesPath, 'icon.ico');

  const createTray = () => new Tray(iconPath);

  it('creates tray and basic lifecycle works', () => {
    const tray = createTray();
    expect(tray.isDestroyed()).to.equal(false);
    tray.setToolTip('test');
    tray.destroy();
    expect(tray.isDestroyed()).to.equal(true);
  });

  it('supports setImage and getBounds', () => {
    const tray = createTray();
    const image = nativeImage.createFromPath(iconPath);
    tray.setImage(image);
    const bounds = tray.getBounds();
    expect(bounds).to.have.property('x');
    expect(bounds).to.have.property('y');
    expect(bounds).to.have.property('width');
    expect(bounds).to.have.property('height');
    tray.destroy();
  });

  it('supports context menu', () => {
    const tray = createTray();
    const menu = Menu.buildFromTemplate([{ label: 'Item1' }]);
    tray.setContextMenu(menu);
    tray.popUpContextMenu();
    tray.closeContextMenu();
    tray.destroy();
  });

  it('returns GUID when provided', () => {
    const guid = '6cf7b0a4-3cd7-4f10-8c11-2a3fa1a1d9aa';
    const tray = new Tray(iconPath, guid);
    expect(tray.getGUID()).to.equal(guid);
    tray.destroy();
  });

  ifdescribe(process.platform === 'darwin')('macOS specific', () => {
    it('supports title APIs', () => {
      const tray = createTray();
      tray.setTitle('T');
      expect(tray.getTitle()).to.equal('T');
      tray.setIgnoreDoubleClickEvents(true);
      expect(tray.getIgnoreDoubleClickEvents()).to.equal(true);
      tray.destroy();
    });
  });

  ifdescribe(process.platform === 'win32')('Windows specific', () => {
    it('supports balloon APIs', () => {
      const tray = createTray();
      tray.displayBalloon({
        title: 'Title',
        content: 'Content',
        iconType: 'info',
      });
      tray.removeBalloon();
      tray.focus();
      tray.destroy();
    });
  });
});
