// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { Menu, Tray, nativeImage } from 'lynxtron';

import { expect } from 'chai';
import * as path from 'node:path';
import { setTimeout } from 'node:timers/promises';

import { ifdescribe } from './lib/spec-helpers';

const isSupportedPlatform =
  process.platform === 'darwin' || process.platform === 'win32';

ifdescribe(isSupportedPlatform)('tray module', () => {
  const fixturesPath = path.join(__dirname, 'fixtures', 'assets');
  const badPath = path.resolve('I', 'Do', 'Not', 'Exist');
  const iconPath =
    process.platform === 'darwin'
      ? path.join(fixturesPath, 'logo_Template.png')
      : path.join(fixturesPath, 'icon.ico');
  const createMenu = () => Menu.buildFromTemplate([{ label: 'Item1' }]);

  let tray: Tray;

  beforeEach(() => {
    tray = new Tray(iconPath);
  });

  afterEach(() => {
    tray.destroy();
    tray = null as any;
  });

  describe('new Tray', () => {
    it('sets the correct class name on the prototype', () => {
      expect(Tray.prototype.constructor.name).to.equal('Tray');
    });

    it('throws a descriptive error for a missing file', () => {
      expect(() => {
        tray = new Tray(badPath);
      }).to.throw(/Failed to load image from path (.+)/);
    });

    it('throws a descriptive error if an invalid guid is given', () => {
      expect(() => {
        tray = new Tray(iconPath, 'I am not a guid');
      }).to.throw(/Invalid GUID format/);
    });

    it('accepts a valid guid', () => {
      expect(() => {
        tray = new Tray(iconPath, '0019A433-3526-48BA-A66C-676742C0FEFB');
      }).to.not.throw();
    });

    it('is an instance of Tray', () => {
      expect(tray).to.be.an.instanceOf(Tray);
    });

    it('returns GUID when provided', () => {
      const guid = '6cf7b0a4-3cd7-4f10-8c11-2a3fa1a1d9aa';
      const trayWithGuid = new Tray(iconPath, guid);
      expect(trayWithGuid.getGUID()).to.equal(guid);
      trayWithGuid.destroy();
    });
  });

  ifdescribe(process.platform === 'darwin')(
    'tray get/set ignoreDoubleClickEvents',
    () => {
      it('returns false by default', () => {
        expect(tray.getIgnoreDoubleClickEvents()).to.equal(false);
      });

      it('can be set to true', () => {
        tray.setIgnoreDoubleClickEvents(true);
        expect(tray.getIgnoreDoubleClickEvents()).to.equal(true);
      });
    }
  );

  describe('tray.setContextMenu(menu)', () => {
    it('accepts both null and Menu as parameters', () => {
      expect(() => {
        tray.setContextMenu(createMenu());
      }).to.not.throw();
      expect(() => {
        tray.setContextMenu(null);
      }).to.not.throw();
    });
  });

  describe('tray.destroy()', () => {
    it('destroys a tray', () => {
      expect(tray.isDestroyed()).to.equal(false);
      tray.destroy();
      expect(tray.isDestroyed()).to.equal(true);
    });
  });

  describe('tray.popUpContextMenu()', () => {
    ifdescribe(process.platform === 'win32')('when menu is showing', () => {
      it('can be called more than once', async () => {
        tray.setContextMenu(createMenu());
        const timeout = setTimeout();
        tray.popUpContextMenu();
        await timeout;
        tray.popUpContextMenu();
      });
    });

    it('can be called with the previously set context menu', () => {
      tray.setContextMenu(createMenu());
      expect(() => {
        tray.popUpContextMenu();
      }).to.not.throw();
    });

    it('can be called with a menu', () => {
      expect(() => {
        tray.popUpContextMenu(createMenu());
      }).to.not.throw();
    });

    it('can be called with a position', () => {
      expect(() => {
        tray.popUpContextMenu({ x: 0, y: 0 } as any);
      }).to.not.throw();
    });

    it('can be called with a menu and a position', () => {
      expect(() => {
        tray.popUpContextMenu(createMenu(), { x: 0, y: 0 } as any);
      }).to.not.throw();
    });

    it('throws an error on invalid arguments', () => {
      expect(() => {
        tray.popUpContextMenu({} as any);
      }).to.throw(/index 0/);
      expect(() => {
        tray.popUpContextMenu(createMenu(), {} as any);
      }).to.throw(/index 1/);
    });

    ifdescribe(process.platform === 'darwin')('when switching menus', () => {
      it('can switch between a temporary menu and the previously set context menu', async () => {
        const defaultMenu = Menu.buildFromTemplate([{ label: 'DefaultItem' }]);
        const temporaryMenu = Menu.buildFromTemplate([
          { label: 'TemporaryItem' },
        ]);

        tray.setContextMenu(defaultMenu);
        tray.popUpContextMenu(temporaryMenu);
        tray.closeContextMenu();
        await setTimeout();

        expect(() => {
          tray.popUpContextMenu();
        }).to.not.throw();

        tray.closeContextMenu();
      });
    });
  });

  ifdescribe(process.platform === 'win32')('tray.closeContextMenu()', () => {
    it('does not crash when no menu is showing', () => {
      expect(() => {
        tray.closeContextMenu();
      }).to.not.throw();
    });

    it('does not crash when called more than once', async () => {
      tray.setContextMenu(createMenu());
      const timeout = setTimeout();
      tray.popUpContextMenu();
      await timeout;
      tray.closeContextMenu();
      tray.closeContextMenu();
    });
  });

  ifdescribe(process.platform === 'darwin')('tray.closeContextMenu()', () => {
    it('does not hang when called immediately after popUpContextMenu', async () => {
      tray.setContextMenu(createMenu());
      tray.popUpContextMenu();
      tray.closeContextMenu();
      await setTimeout();
    });
  });

  describe('tray.getBounds()', () => {
    it('returns a bounds object', () => {
      const bounds = tray.getBounds();
      expect(bounds)
        .to.be.an('object')
        .and.to.have.all.keys('x', 'y', 'width', 'height');
    });
  });

  describe('tray.setImage(image)', () => {
    it('accepts an image created from path', () => {
      tray.setImage(nativeImage.createFromPath(iconPath));
    });

    it('accepts empty image', () => {
      tray.setImage(nativeImage.createEmpty());
    });

    it('throws a descriptive error for a missing file', () => {
      expect(() => {
        tray.setImage(badPath);
      }).to.throw(/Failed to load image from path (.+)/);
    });
  });

  describe('tray.setPressedImage(image)', () => {
    it('accepts an image created from path', () => {
      tray.setPressedImage(nativeImage.createFromPath(iconPath));
    });

    it('accepts empty image', () => {
      tray.setPressedImage(nativeImage.createEmpty());
    });

    it('throws a descriptive error for a missing file', () => {
      expect(() => {
        tray.setPressedImage(badPath);
      }).to.throw(/Failed to load image from path (.+)/);
    });
  });

  ifdescribe(process.platform === 'win32')(
    'tray.displayBalloon(options)',
    () => {
      it('supports balloon APIs', () => {
        tray.displayBalloon({
          title: 'Title',
          content: 'Content',
          iconType: 'info',
        });
        tray.removeBalloon();
        tray.focus();
      });

      it('throws a descriptive error for a missing file icon', () => {
        expect(() => {
          tray.displayBalloon({
            title: 'Title',
            content: 'Content',
            icon: badPath,
          });
        }).to.throw(/Failed to load image from path (.+)/);
      });

      it('accepts an empty image icon', () => {
        tray.displayBalloon({
          title: 'Title',
          content: 'Content',
          icon: nativeImage.createEmpty(),
        });
      });
    }
  );

  ifdescribe(process.platform === 'darwin')('tray get/set title', () => {
    it('sets/gets non-empty title', () => {
      tray.setTitle('T');
      expect(tray.getTitle()).to.equal('T');
    });

    it('sets/gets empty title', () => {
      tray.setTitle('');
      expect(tray.getTitle()).to.equal('');
    });

    it('can have an options object passed in', () => {
      expect(() => {
        tray.setTitle('Hello World!', {});
      }).to.not.throw();
    });

    it('throws when the options parameter is not an object', () => {
      expect(() => {
        tray.setTitle('Hello World!', 'test' as any);
      }).to.throw(/setTitle options must be an object/);
    });

    it('can have a font type option set', () => {
      expect(() => {
        tray.setTitle('Hello World!', { fontType: 'monospaced' });
        tray.setTitle('Hello World!', { fontType: 'monospacedDigit' });
      }).to.not.throw();
    });

    it('throws when the font type is specified but is not a string', () => {
      expect(() => {
        tray.setTitle('Hello World!', { fontType: 5.4 as any });
      }).to.throw(/fontType must be one of 'monospaced' or 'monospacedDigit'/);
    });

    it('throws on invalid font types', () => {
      expect(() => {
        tray.setTitle('Hello World!', { fontType: 'blep' as any });
      }).to.throw(/fontType must be one of 'monospaced' or 'monospacedDigit'/);
    });
  });
});
