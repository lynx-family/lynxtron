// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { app, BaseWindow, dialog, LynxWindow } from 'lynxtron';

import { expect } from 'chai';

// @ts-expect-error
import * as childProcess from 'node:child_process';
import { once } from 'node:events';
import { setTimeout } from 'node:timers/promises';

const expectedNativeHandleSize = process.arch === 'ia32' ? 4 : 8;
const expectBoundsEqual = (actual: any, expected: any) => {
  expect(actual).to.deep.equal(expected);
};
// @ts-expect-error
import { ifit, ifdescribe, defer, listen, waitUntil } from './lib/spec-helpers';
import { closeWindow, closeAllWindows } from './lib/window-helpers';

describe('LynxWindow module', () => {
  it('sets the correct class name on the prototype', () => {
    expect(LynxWindow.prototype.constructor.name).to.equal('LynxWindow');
  });

  describe('LynxWindow constructor', () => {
    it('allows show false', async () => {
      expect(() => {
        const w = new LynxWindow({
          show: false,
        } as any);
        w.destroy();
      }).not.to.throw();
    });

    ifit(process.platform === 'darwin')('supports tabbingIdentifier', () => {
      const w = new LynxWindow({
        show: false,
        tabbingIdentifier: 'tab-group',
      });
      expect(w.tabbingIdentifier).to.equal('tab-group');
      w.destroy();
    });

    ifdescribe(process.platform === 'darwin')('native tabs', () => {
      afterEach(closeAllWindows);

      it.skip('exposes native tab methods', async () => {
        const w1 = new LynxWindow({
          show: false,
          tabbingIdentifier: 'tab-group',
        });
        const w2 = new LynxWindow({
          show: false,
          tabbingIdentifier: 'tab-group',
        });

        try {
          expect(() => {
            w1.addTabbedWindow(w2);
            w1.selectNextTab();
            w1.selectPreviousTab();
            w1.mergeAllWindows();
            w1.moveTabToNewWindow();
            w1.toggleTabBar();
            w1.showAllTabs();
          }).not.to.throw();
        } finally {
          await closeWindow(w1, { assertNotWindows: false });
          await closeWindow(w2, { assertNotWindows: false });
          await closeAllWindows();
        }
      });
    });

    it('does not crash or throw when passed an invalid icon', async () => {
      expect(() => {
        const w = new LynxWindow({
          icon: undefined,
        } as any);
        w.destroy();
      }).not.to.throw();
    });

    describe('sizing semantics', () => {
      it('defaults to an 800x600 window and centers when no position is given', async () => {
        const wDefault = new LynxWindow({ show: false });
        const wCentered = new LynxWindow({
          show: false,
          x: 10,
          y: 10,
          width: 800,
          height: 600,
        });

        try {
          expectBoundsEqual(wDefault.getSize(), [800, 600]);

          const before = wCentered.getPosition();
          wCentered.center();
          await waitUntil(() => {
            const after = wCentered.getPosition();
            return after[0] !== before[0] || after[1] !== before[1];
          });

          const defaultPosition = wDefault.getPosition();
          const centeredPosition = wCentered.getPosition();
          expect(
            Math.abs(defaultPosition[0] - centeredPosition[0])
          ).to.be.at.most(2);
          expect(
            Math.abs(defaultPosition[1] - centeredPosition[1])
          ).to.be.at.most(2);
        } finally {
          await closeWindow(wCentered, { assertNotWindows: false });
          await closeWindow(wDefault, { assertNotWindows: false });
        }
      });

      ifit(process.platform === 'win32')(
        'returns constructor window size in DIP on Windows',
        async () => {
          const w = new LynxWindow({
            show: false,
            width: 320,
            height: 240,
          });

          try {
            expectBoundsEqual(w.getSize(), [320, 240]);
            const bounds = w.getBounds();
            expectBoundsEqual([bounds.width, bounds.height], [320, 240]);
          } finally {
            await closeWindow(w, { assertNotWindows: false });
          }
        }
      );

      it('uses width and height as content bounds when useContentSize is true', async () => {
        const w = new LynxWindow({
          show: false,
          width: 420,
          height: 320,
          useContentSize: true,
        });

        try {
          expectBoundsEqual(w.getContentSize(), [420, 320]);

          const [windowWidth, windowHeight] = w.getSize();
          expect(windowWidth).to.be.at.least(420);
          expect(windowHeight).to.be.at.least(320);
        } finally {
          await closeWindow(w, { assertNotWindows: false });
        }
      });

      it('applies constructor minimum size in content space when useContentSize is true', async () => {
        const w = new LynxWindow({
          show: false,
          width: 400,
          height: 300,
          useContentSize: true,
          minWidth: 300,
          minHeight: 320,
        });

        try {
          w.setContentSize(200, 200);
          await waitUntil(() => {
            const [width, height] = w.getContentSize();
            return width >= 300 && height >= 320;
          });

          const [width, height] = w.getContentSize();
          expect(width).to.be.at.least(300);
          expect(height).to.be.at.least(320);
        } finally {
          await closeWindow(w, { assertNotWindows: false });
        }
      });

      it('applies constructor maximum size in content space when useContentSize is true', async () => {
        const w = new LynxWindow({
          show: false,
          width: 320,
          height: 300,
          useContentSize: true,
          maxWidth: 260,
          maxHeight: 240,
        });

        try {
          w.setContentSize(500, 480);
          await waitUntil(() => {
            const [width, height] = w.getContentSize();
            return width <= 260 && height <= 240;
          });

          const [width, height] = w.getContentSize();
          expect(width).to.be.at.most(260);
          expect(height).to.be.at.most(240);
        } finally {
          await closeWindow(w, { assertNotWindows: false });
        }
      });
    });
  });

  describe('background color', () => {
    it('applies backgroundColor from constructor and can be read back', async () => {
      const w = new LynxWindow({
        show: false,
        backgroundColor: '#FF0000',
      } as any);
      try {
        const hex = w.getBackgroundColor();
        expect(hex).to.equal('#FF0000');
      } finally {
        w.destroy();
      }
    });

    it('setBackgroundColor updates getBackgroundColor at runtime', async () => {
      const w = new LynxWindow({ show: false } as any);
      try {
        w.setBackgroundColor('#00FF00');
        const hex = w.getBackgroundColor();
        expect(hex).to.equal('#00FF00');
      } finally {
        w.destroy();
      }
    });
  });

  describe('garbage collection', () => {
    const v8Util = process._linkedBinding('lynxtron_binding_v8_util');
    afterEach(closeAllWindows);

    it('window does not get garbage collected when opened', async () => {
      const w = new LynxWindow({ show: false });
      // Keep a weak reference to the window.
      const wr = new WeakRef(w);
      await setTimeout();
      // Do garbage collection, since |w| is not referenced in this closure
      // it would be gone after next call if there is no other reference.
      v8Util.requestGarbageCollectionForTesting();

      await setTimeout();
      expect(wr.deref()).to.not.be.undefined();
    });
  });

  describe('LynxWindow.close()', () => {
    let w: LynxWindow;
    beforeEach(() => {
      w = new LynxWindow({ show: false });
    });
    afterEach(async () => {
      await closeWindow(w);
      w = (null as unknown) as LynxWindow;
    });

    it('should work if called when a messageBox is showing', async () => {
      const closed = once(w, 'closed');
      void dialog.showMessageBox(w, { message: 'Hello Error' });
      w.close();
      await closed;
    });

    it('should work if called when multiple messageBoxes are showing', async () => {
      const closed = once(w, 'closed');
      void dialog.showMessageBox(w, { message: 'Hello Error' });
      void dialog.showMessageBox(w, { message: 'Hello Error' });
      w.close();
      await closed;
    });

    it('closes window without rounded corners', async () => {
      await closeWindow(w);
      w = new LynxWindow({ show: false, frame: false, roundedCorners: false });
      const closed = once(w, 'closed');
      w.close();
      await closed;
    });

    it('should allow access to id after destruction', async () => {
      const closed = once(w, 'closed');
      w.destroy();
      await closed;
      expect(w.id).to.be.a('number');
    });
  });

  describe('LynxWindow.destroy()', () => {
    let w: LynxWindow;
    beforeEach(() => {
      w = new LynxWindow({ show: false });
    });
    afterEach(async () => {
      await closeWindow(w);
      w = (null as unknown) as LynxWindow;
    });

    it('should not crash when destroying windows with pending events', () => {
      const focusListener = () => {};
      app.on('lynx-window-focus', focusListener);
      const windowCount = 3;
      const windowOptions = {
        show: false,
        width: 400,
        height: 400,
      };
      const windows = Array.from(Array(windowCount)).map(
        () => new LynxWindow(windowOptions)
      );
      for (const win of windows) win.show();
      for (const win of windows) win.focus();
      for (const win of windows) win.destroy();
      app.removeListener('lynx-window-focus', focusListener);
    });

    describe('focus and visibility', () => {
      let w: LynxWindow;
      beforeEach(() => {
        w = new LynxWindow({ show: false });
      });
      afterEach(async () => {
        await closeWindow(w);
        w = (null as unknown) as LynxWindow;
      });

      describe('LynxWindow.show()', () => {
        it('should focus on window', async () => {
          const p = once(w, 'focus');
          w.show();
          await p;
          expect(w.isFocused()).to.equal(true);
        });
        it('should make the window visible', async () => {
          const p = once(w, 'focus');
          w.show();
          await p;
          expect(w.isVisible()).to.equal(true);
        });
        it('emits when window is shown', async () => {
          const show = once(w, 'show');
          w.show();
          await show;
          expect(w.isVisible()).to.equal(true);
        });
        it('BaseWindow.getFocusedWindow returns focused window', async () => {
          const p = once(w, 'focus');
          w.show();
          await p;
          expect(BaseWindow.getFocusedWindow()).to.equal(w);
        });
      });

      describe('LynxWindow.hide()', () => {
        it('should defocus on window', () => {
          w.hide();
          expect(w.isFocused()).to.equal(false);
        });
        it('should make the window not visible', () => {
          w.show();
          w.hide();
          expect(w.isVisible()).to.equal(false);
        });
        it('emits when window is hidden', async () => {
          const shown = once(w, 'show');
          w.show();
          await shown;
          const hidden = once(w, 'hide');
          w.hide();
          await hidden;
          expect(w.isVisible()).to.equal(false);
        });
      });

      describe('LynxWindow.minimize()', () => {
        // TODO(codebytere): Enable for Linux once maximize/minimize events work in CI.
        ifit(process.platform !== 'linux')(
          'should not be visible when the window is minimized',
          async () => {
            const minimize = once(w, 'minimize');
            w.minimize();
            await minimize;

            expect(w.isMinimized()).to.equal(true);
            expect(w.isVisible()).to.equal(false);
          }
        );

        ifit(process.platform !== 'linux')(
          'does not emit hide when the window is minimized',
          async () => {
            let hideCount = 0;
            w.on('hide', () => {
              hideCount++;
            });

            const shown = once(w, 'show');
            w.show();
            await shown;

            const minimize = once(w, 'minimize');
            w.minimize();
            await minimize;
            await setTimeout(100);

            expect(hideCount).to.equal(0);
            expect(w.isMinimized()).to.equal(true);
          }
        );
      });

      describe('LynxWindow.restore()', () => {
        ifit(process.platform !== 'linux')(
          'does not emit show when restoring a minimized window',
          async () => {
            let showCount = 0;
            w.on('show', () => {
              showCount++;
            });

            const shown = once(w, 'show');
            w.show();
            await shown;
            expect(showCount).to.equal(1);

            const minimize = once(w, 'minimize');
            w.minimize();
            await minimize;

            const restored = once(w, 'restore');
            w.restore();
            await restored;
            await setTimeout(100);

            expect(showCount).to.equal(1);
            expect(w.isVisible()).to.equal(true);
            expect(w.isMinimized()).to.equal(false);
          }
        );

        ifit(process.platform !== 'linux')(
          'restores a maximized window after minimize',
          async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            const maximized = once(w, 'maximize');
            w.maximize();
            await maximized;
            expect(w.isMaximized()).to.equal(true);

            const minimized = once(w, 'minimize');
            w.minimize();
            await minimized;
            expect(w.isMinimized()).to.equal(true);
            expect(w.isMaximized()).to.equal(false);

            const restored = once(w, 'restore');
            w.restore();
            await restored;

            expect(w.isMinimized()).to.equal(false);
            expect(w.isMaximized()).to.equal(true);
          }
        );
      });

      describe('LynxWindow.unmaximize()', () => {
        ifit(process.platform !== 'linux')(
          'does not restore a minimized window',
          async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            const minimized = once(w, 'minimize');
            w.minimize();
            await minimized;

            w.unmaximize();
            await setTimeout(50);

            expect(w.isMinimized()).to.equal(true);
          }
        );

        it('does not change the size or position of a normal window', async () => {
          const initialSize = w.getSize();
          const initialPosition = w.getPosition();

          w.unmaximize();
          await setTimeout(50);

          expectBoundsEqual(w.getSize(), initialSize);
          expectBoundsEqual(w.getPosition(), initialPosition);
        });

        ifit(process.platform !== 'linux')(
          'does not change size for a frameless window with min size',
          async () => {
            await closeWindow(w, { assertNotWindows: false });
            w = new LynxWindow({
              show: false,
              frame: false,
              width: 300,
              height: 300,
              minWidth: 300,
              minHeight: 300,
            });
            const bounds = w.getBounds();

            w.once('maximize', () => {
              w.unmaximize();
            });
            const unmaximize = once(w, 'unmaximize');
            w.show();
            await waitUntil(() => w.isVisible());
            w.maximize();
            await unmaximize;

            expectBoundsEqual(w.getNormalBounds(), bounds);
          }
        );
      });

      describe('LynxWindow.isMaximized()', () => {
        ifit(process.platform !== 'linux')(
          'correctly checks transparent window maximization state',
          async () => {
            await closeWindow(w, { assertNotWindows: false });
            w = new LynxWindow({
              show: false,
              width: 300,
              height: 300,
              transparent: true,
            });

            const maximize = once(w, 'maximize');
            w.show();
            await waitUntil(() => w.isVisible());
            w.maximize();
            await maximize;
            expect(w.isMaximized()).to.equal(true);

            const unmaximize = once(w, 'unmaximize');
            w.unmaximize();
            await unmaximize;
            expect(w.isMaximized()).to.equal(false);
          }
        );

        ifit(process.platform !== 'linux')(
          'returns true for windows with an aspect ratio after resizable becomes false',
          async () => {
            await closeWindow(w, { assertNotWindows: false });
            w = new LynxWindow({
              show: false,
              fullscreenable: false,
            });

            w.setAspectRatio(16 / 11);

            const maximize = once(w, 'maximize');
            w.show();
            await waitUntil(() => w.isVisible());
            w.maximize();
            await maximize;

            expect(w.isMaximized()).to.equal(true);
            w.resizable = false;
            expect(w.isMaximized()).to.equal(true);
          }
        );
      });

      describe('LynxWindow.showInactive()', () => {
        it('should not focus on window', () => {
          w.showInactive();
          expect(w.isFocused()).to.equal(false);
        });

        // TODO(dsanders11): Enable for Linux once CI plays nice with these kinds of tests
        ifit(process.platform !== 'linux')(
          'should not restore maximized windows',
          async () => {
            const maximize = once(w, 'maximize');
            const shown = once(w, 'show');
            w.maximize();
            // TODO(dsanders11): The maximize event isn't firing on macOS for a window initially hidden
            if (process.platform !== 'darwin') {
              await maximize;
            } else {
              await setTimeout(1000);
            }
            w.showInactive();
            await shown;
            expect(w.isMaximized()).to.equal(true);
          }
        );

        ifit(process.platform === 'darwin')(
          'should attach child window to parent',
          async () => {
            const wShow = once(w, 'show');
            w.show();
            await wShow;

            const c = new LynxWindow({ show: false, parent: w });
            const cShow = once(c, 'show');
            c.showInactive();
            await cShow;

            // verifying by checking that the child tracks the parent's visibility
            const minimized = once(w, 'minimize');
            w.minimize();
            await minimized;

            expect(w.isVisible()).to.be.false('parent is visible');
            expect(c.isVisible()).to.be.false('child is visible');

            const restored = once(w, 'restore');
            w.restore();
            await restored;

            expect(w.isVisible()).to.be.true('parent is visible');
            expect(c.isVisible()).to.be.true('child is visible');

            closeWindow(c);
          }
        );
      });

      describe('LynxWindow.focus()', () => {
        it('does not make the window become visible', () => {
          expect(w.isVisible()).to.equal(false);
          w.focus();
          expect(w.isVisible()).to.equal(false);
        });

        // FIXME(Guo Xi): failed in electron
        ifit(process.platform !== 'win32')(
          'focuses a blurred window',
          async () => {
            {
              const isBlurred = once(w, 'blur');
              const isShown = once(w, 'show');
              w.show();
              w.blur();
              await isShown;
              await isBlurred;
            }
            expect(w.isFocused()).to.equal(false);
            w.focus();
            await waitUntil(() => w.isFocused());
            expect(w.isFocused()).to.equal(true);
          }
        );

        ifit(process.platform !== 'linux')(
          'acquires focus status from the other windows',
          async () => {
            const w1 = new LynxWindow({ show: false });
            const w2 = new LynxWindow({ show: false });
            const w3 = new LynxWindow({ show: false });
            {
              const isFocused3 = once(w3, 'focus');
              const isShown1 = once(w1, 'show');
              const isShown2 = once(w2, 'show');
              const isShown3 = once(w3, 'show');
              w1.show();
              w2.show();
              w3.show();
              await isShown1;
              await isShown2;
              await isShown3;
              await isFocused3;
            }
            // TODO(RaisinTen): Investigate why this assertion fails only on Linux.
            expect(w1.isFocused()).to.equal(false);
            expect(w2.isFocused()).to.equal(false);
            expect(w3.isFocused()).to.equal(true);

            w1.focus();
            expect(w1.isFocused()).to.equal(true);
            expect(w2.isFocused()).to.equal(false);
            expect(w3.isFocused()).to.equal(false);

            w2.focus();
            expect(w1.isFocused()).to.equal(false);
            expect(w2.isFocused()).to.equal(true);
            expect(w3.isFocused()).to.equal(false);

            w3.focus();
            expect(w1.isFocused()).to.equal(false);
            expect(w2.isFocused()).to.equal(false);
            expect(w3.isFocused()).to.equal(true);

            {
              const isClosed1 = once(w1, 'closed');
              const isClosed2 = once(w2, 'closed');
              const isClosed3 = once(w3, 'closed');
              w1.destroy();
              w2.destroy();
              w3.destroy();
              await isClosed1;
              await isClosed2;
              await isClosed3;
            }
          }
        );
      });

      // TODO(RaisinTen): Make this work on Windows too.
      // Refs: https://github.com/electron/electron/issues/20464.
      ifdescribe(process.platform !== 'win32')('LynxWindow.blur()', () => {
        it('removes focus from window', async () => {
          {
            const isFocused = once(w, 'focus');
            const isShown = once(w, 'show');
            w.show();
            await isShown;
            await isFocused;
          }
          expect(w.isFocused()).to.equal(true);
          w.blur();
          expect(w.isFocused()).to.equal(false);
        });

        it('transfers focus status to the next window', async () => {
          const w1 = new LynxWindow({ show: false });
          const w2 = new LynxWindow({ show: false });
          const w3 = new LynxWindow({ show: false });
          {
            const isFocused3 = once(w3, 'focus');
            const isShown1 = once(w1, 'show');
            const isShown2 = once(w2, 'show');
            const isShown3 = once(w3, 'show');
            w1.show();
            w2.show();
            w3.show();
            await isShown1;
            await isShown2;
            await isShown3;
            await isFocused3;
          }
          // TODO(RaisinTen): Investigate why this assertion fails only on Linux.
          expect(w1.isFocused()).to.equal(false);
          expect(w2.isFocused()).to.equal(false);
          expect(w3.isFocused()).to.equal(true);

          w3.blur();
          expect(w1.isFocused()).to.equal(false);
          expect(w2.isFocused()).to.equal(true);
          expect(w3.isFocused()).to.equal(false);

          w2.blur();
          expect(w1.isFocused()).to.equal(true);
          expect(w2.isFocused()).to.equal(false);
          expect(w3.isFocused()).to.equal(false);

          w1.blur();
          expect(w1.isFocused()).to.equal(false);
          expect(w2.isFocused()).to.equal(false);
          expect(w3.isFocused()).to.equal(true);

          {
            const isClosed1 = once(w1, 'closed');
            const isClosed2 = once(w2, 'closed');
            const isClosed3 = once(w3, 'closed');
            w1.destroy();
            w2.destroy();
            w3.destroy();
            await isClosed1;
            await isClosed2;
            await isClosed3;
          }
        });
      });

      ifdescribe(process.platform !== 'linux')('LynxWindow.fullscreen', () => {
        let w: LynxWindow;
        beforeEach(() => {
          w = new LynxWindow({ show: false });
        });
        afterEach(async () => {
          await closeWindow(w);
          w = (null as unknown) as LynxWindow;
        });

        it('can be changed with the fullScreen property', async () => {
          const shown = once(w, 'show');
          w.show();
          await shown;

          {
            const events: string[] = [];
            w.once('will-enter-full-screen', () => events.push('will-enter'));
            w.once('enter-full-screen', () => events.push('enter'));
            w.fullScreen = true;
            await waitUntil(() => events.length === 2);
            expect(events).to.deep.equal(['will-enter', 'enter']);
          }

          expect(w.fullScreen).to.equal(true);

          {
            const events: string[] = [];
            w.once('will-leave-full-screen', () => events.push('will-leave'));
            w.once('leave-full-screen', () => events.push('leave'));
            w.fullScreen = false;
            await waitUntil(() => events.length === 2);
            expect(events).to.deep.equal(['will-leave', 'leave']);
          }

          expect(w.fullScreen).to.equal(false);
        });

        it('can be changed with setFullScreen', async () => {
          const shown = once(w, 'show');
          w.show();
          await shown;

          const enterFullScreen = once(w, 'enter-full-screen');
          w.setFullScreen(true);
          await enterFullScreen;
          expect(w.isFullScreen()).to.equal(true);

          const leaveFullScreen = once(w, 'leave-full-screen');
          w.setFullScreen(false);
          await leaveFullScreen;
          expect(w.isFullScreen()).to.equal(false);
        });

        it('reports not maximized after maximizing then fullscreening', async () => {
          const shown = once(w, 'show');
          w.show();
          await shown;

          const maximize = once(w, 'maximize');
          w.maximize();
          await maximize;
          expect(w.isMaximized()).to.equal(true);

          const enterFullScreen = once(w, 'enter-full-screen');
          w.setFullScreen(true);
          await enterFullScreen;

          expect(w.isMaximized()).to.equal(false);
          expect(w.isFullScreen()).to.equal(true);

          const leaveFullScreen = once(w, 'leave-full-screen');
          w.setFullScreen(false);
          await leaveFullScreen;
        });
      });

      ifdescribe(process.platform === 'darwin')(
        'LynxWindow.simpleFullScreen',
        () => {
          let w: LynxWindow;
          beforeEach(() => {
            w = new LynxWindow({ show: false });
          });
          afterEach(async () => {
            await closeWindow(w);
            w = (null as unknown) as LynxWindow;
          });

          it('can be changed with the simpleFullScreen property', async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            w.simpleFullScreen = true;
            await waitUntil(() => w.isSimpleFullScreen());
            expect(w.simpleFullScreen).to.equal(true);

            w.simpleFullScreen = false;
            await waitUntil(() => !w.isSimpleFullScreen());
            expect(w.simpleFullScreen).to.equal(false);
          });

          it('can be changed with setSimpleFullScreen', async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            w.setSimpleFullScreen(true);
            await waitUntil(() => w.isSimpleFullScreen());
            expect(w.isSimpleFullScreen()).to.equal(true);

            w.setSimpleFullScreen(false);
            await waitUntil(() => !w.isSimpleFullScreen());
            expect(w.isSimpleFullScreen()).to.equal(false);
          });

          it('does not crash when exiting simpleFullScreen via setFullScreen', async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            w.setSimpleFullScreen(true);
            await waitUntil(() => w.isSimpleFullScreen());

            w.setFullScreen(false);
            await waitUntil(() => !w.isSimpleFullScreen());
            expect(w.isSimpleFullScreen()).to.equal(false);
          });

          it('honors simpleFullscreen constructor option with setFullScreen', async () => {
            await closeWindow(w, { assertNotWindows: false });
            w = new LynxWindow({ show: false, simpleFullscreen: true });
            w.show();
            await waitUntil(() => w.isVisible());

            w.setFullScreen(true);
            await waitUntil(() => w.isSimpleFullScreen());
            expect(w.isSimpleFullScreen()).to.equal(true);

            w.setFullScreen(false);
            await waitUntil(() => !w.isSimpleFullScreen());
            expect(w.isSimpleFullScreen()).to.equal(false);
          });
        }
      );

      ifdescribe(process.platform === 'darwin')('fullscreenable state', () => {
        afterEach(closeAllWindows);

        describe('with functions', () => {
          it('can be set with fullscreenable constructor option', () => {
            const w = new LynxWindow({ show: false, fullscreenable: false });
            expect(w.isFullScreenable()).to.be.false('isFullScreenable');
          });

          it('can be changed', () => {
            const w = new LynxWindow({ show: false });
            expect(w.isFullScreenable()).to.be.true('isFullScreenable');
            w.setFullScreenable(false);
            expect(w.isFullScreenable()).to.be.false('isFullScreenable');
            w.setFullScreenable(true);
            expect(w.isFullScreenable()).to.be.true('isFullScreenable');
          });
        });

        describe('with property', () => {
          it('can be changed through the fullScreenable property', () => {
            const w = new LynxWindow({ show: false });
            expect(w.fullScreenable).to.be.true('fullScreenable');
            w.fullScreenable = false;
            expect(w.fullScreenable).to.be.false('fullScreenable');
            expect(w.isFullScreenable()).to.be.false('isFullScreenable');
            w.fullScreenable = true;
            expect(w.fullScreenable).to.be.true('fullScreenable');
          });
        });

        it('is set correctly with different resizable values', () => {
          const w1 = new LynxWindow({
            show: false,
            resizable: false,
            fullscreenable: false,
          });
          const w2 = new LynxWindow({
            show: false,
            resizable: true,
            fullscreenable: false,
          });
          const w3 = new LynxWindow({
            show: false,
            fullscreenable: false,
          });

          expect(w1.isFullScreenable()).to.be.false('isFullScreenable');
          expect(w2.isFullScreenable()).to.be.false('isFullScreenable');
          expect(w3.isFullScreenable()).to.be.false('isFullScreenable');
        });

        it('does not disable maximize button if window is resizable', () => {
          const w = new LynxWindow({
            show: false,
            resizable: true,
            fullscreenable: false,
          });

          expect(w.isMaximizable()).to.be.true('isMaximizable');

          w.setResizable(false);

          expect(w.isMaximizable()).to.be.false('isMaximizable');
        });
      });

      ifdescribe(process.platform === 'darwin')(
        'isHiddenInMissionControl state',
        () => {
          afterEach(closeAllWindows);

          describe('with functions', () => {
            it('can be set with hiddenInMissionControl constructor option', () => {
              const w = new LynxWindow({
                show: false,
                hiddenInMissionControl: true,
              });
              expect(w.isHiddenInMissionControl()).to.be.true(
                'isHiddenInMissionControl'
              );
            });

            it('can be changed', () => {
              const w = new LynxWindow({ show: false });
              expect(w.isHiddenInMissionControl()).to.be.false(
                'isHiddenInMissionControl'
              );
              w.setHiddenInMissionControl(true);
              expect(w.isHiddenInMissionControl()).to.be.true(
                'isHiddenInMissionControl'
              );
              w.setHiddenInMissionControl(false);
              expect(w.isHiddenInMissionControl()).to.be.false(
                'isHiddenInMissionControl'
              );
            });
          });
        }
      );

      ifdescribe(process.platform === 'darwin')('LynxWindow vibrancy', () => {
        afterEach(closeAllWindows);

        describe('lifecycle', () => {
          it('creates and removes a vibrancy view', async () => {
            const w = new LynxWindow({ show: false });
            const shown = once(w, 'show');
            w.show();
            await shown;

            expect((w as any)._hasVibrancyView()).to.equal(false);

            w.setVibrancy('sidebar');
            await waitUntil(() => (w as any)._hasVibrancyView() === true);
            expect((w as any)._getVisualEffectState()).to.equal('followWindow');
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'followWindow'
            );

            w.setVibrancy(null);
            await waitUntil(() => (w as any)._hasVibrancyView() === false);
            expect((w as any)._getNativeVisualEffectState()).to.equal('none');
          });

          it('updates the vibrancy type without removing the view', async () => {
            const w = new LynxWindow({ show: false });

            w.setVibrancy('sidebar');
            await waitUntil(() => (w as any)._hasVibrancyView() === true);
            expect((w as any)._getVibrancyType()).to.equal('sidebar');

            w.setVibrancy('menu');
            expect((w as any)._hasVibrancyView()).to.equal(true);
            expect((w as any)._getVibrancyType()).to.equal('menu');

            w.setVibrancy(null);
            await waitUntil(() => (w as any)._hasVibrancyView() === false);
            expect((w as any)._getVibrancyType()).to.equal('');
          });

          it('supports animationDuration when creating and removing vibrancy', async () => {
            const w = new LynxWindow({ show: false });
            const shown = once(w, 'show');
            w.show();
            await shown;

            (w as any).setVibrancy('sidebar', { animationDuration: 50 });
            await waitUntil(() => (w as any)._hasVibrancyView() === true);
            expect((w as any)._getVibrancyType()).to.equal('sidebar');

            (w as any).setVibrancy(null, { animationDuration: 50 });
            await waitUntil(() => (w as any)._hasVibrancyView() === false);
            expect((w as any)._getNativeVisualEffectState()).to.equal('none');
          });
        });

        describe('visualEffectState', () => {
          it('applies constructor visualEffectState when creating vibrancy', async () => {
            const w = new LynxWindow({
              show: false,
              vibrancy: 'sidebar',
              visualEffectState: 'inactive',
            });

            expect((w as any)._hasVibrancyView()).to.equal(true);
            expect((w as any)._getVisualEffectState()).to.equal('inactive');
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'inactive'
            );

            w.setVibrancy(null);
            await waitUntil(() => (w as any)._hasVibrancyView() === false);
          });

          it('defaults constructor visualEffectState to followWindow', () => {
            const w = new LynxWindow({
              show: false,
              vibrancy: 'sidebar',
            });

            expect((w as any)._hasVibrancyView()).to.equal(true);
            expect((w as any)._getVisualEffectState()).to.equal('followWindow');
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'followWindow'
            );
          });

          it('applies the active visualEffectState', () => {
            const w = new LynxWindow({
              show: false,
              vibrancy: 'sidebar',
              visualEffectState: 'active',
            });

            expect((w as any)._hasVibrancyView()).to.equal(true);
            expect((w as any)._getVisualEffectState()).to.equal('active');
            expect((w as any)._getNativeVisualEffectState()).to.equal('active');
          });
        });

        describe('window transitions', () => {
          it('keeps followWindow native state across focus changes', async () => {
            const w = new LynxWindow({
              show: false,
              vibrancy: 'sidebar',
            });
            const other = new LynxWindow({ show: false });

            const shown = once(w, 'show');
            w.show();
            await shown;
            const otherShown = once(other, 'show');
            other.show();
            await otherShown;

            w.focus();
            await waitUntil(() => w.isFocused());
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'followWindow'
            );

            other.focus();
            await waitUntil(() => other.isFocused());
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'followWindow'
            );
          });

          it('keeps the active visualEffectState after focus changes', async () => {
            const w = new LynxWindow({
              show: false,
              vibrancy: 'sidebar',
              visualEffectState: 'active',
            });
            const other = new LynxWindow({ show: false });

            const shown = once(w, 'show');
            w.show();
            await shown;
            const otherShown = once(other, 'show');
            other.show();
            await otherShown;

            other.focus();
            await waitUntil(() => other.isFocused());
            expect((w as any)._getNativeVisualEffectState()).to.equal('active');
          });

          it('keeps the inactive visualEffectState after focus changes', async () => {
            const w = new LynxWindow({
              show: false,
              vibrancy: 'sidebar',
              visualEffectState: 'inactive',
            });
            const other = new LynxWindow({ show: false });

            const shown = once(w, 'show');
            w.show();
            await shown;
            const otherShown = once(other, 'show');
            other.show();
            await otherShown;

            w.focus();
            await waitUntil(() => w.isFocused());
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'inactive'
            );

            other.focus();
            await waitUntil(() => other.isFocused());
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'inactive'
            );
          });

          it('keeps vibrancy through fullscreen transitions', async () => {
            const w = new LynxWindow({
              show: false,
              vibrancy: 'sidebar',
              visualEffectState: 'followWindow',
            });

            const shown = once(w, 'show');
            w.show();
            await shown;
            expect((w as any)._hasVibrancyView()).to.equal(true);
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'followWindow'
            );

            const enterFullScreen = once(w, 'enter-full-screen');
            w.setFullScreen(true);
            await enterFullScreen;
            expect((w as any)._hasVibrancyView()).to.equal(true);
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'followWindow'
            );

            const leaveFullScreen = once(w, 'leave-full-screen');
            w.setFullScreen(false);
            await leaveFullScreen;
            expect((w as any)._hasVibrancyView()).to.equal(true);
            expect((w as any)._getNativeVisualEffectState()).to.equal(
              'followWindow'
            );
          });
        });
      });

      ifdescribe(process.platform === 'darwin')(
        'LynxWindow.setWindowButtonVisibility()',
        () => {
          afterEach(closeAllWindows);

          it('correctly updates when entering/exiting fullscreen for hidden style', async () => {
            const w = new LynxWindow({
              show: false,
              frame: false,
              titleBarStyle: 'hidden',
            });
            expect(w._getWindowButtonVisibility()).to.equal(true);
            w.setWindowButtonVisibility(false);
            expect(w._getWindowButtonVisibility()).to.equal(false);

            const shown = once(w, 'show');
            w.show();
            await shown;

            const enterFullScreen = once(w, 'enter-full-screen');
            w.setFullScreen(true);
            await enterFullScreen;

            const leaveFullScreen = once(w, 'leave-full-screen');
            w.setFullScreen(false);
            await leaveFullScreen;

            w.setWindowButtonVisibility(true);
            expect(w._getWindowButtonVisibility()).to.equal(true);
          });
        }
      );

      ifdescribe(process.platform === 'darwin')(
        'LynxWindow titleBarStyle hiddenInset size preservation',
        () => {
          afterEach(closeAllWindows);

          // Regression: switching FullSizeContentView after setFrame used to
          // make AppKit shrink the content by a title bar height, so a
          // 1512x870 window ended up delivering 1512x837 to the content view.
          it('keeps the requested size when using hiddenInset + useContentSize=false', () => {
            const w = new LynxWindow({
              show: false,
              width: 1512,
              height: 870,
              titleBarStyle: 'hiddenInset',
            });
            const bounds = w.getBounds();
            const content = w.getContentBounds();
            expect(bounds.width).to.equal(1512);
            expect(bounds.height).to.equal(870);
            // With FullSizeContentView the content view must equal the frame,
            // not frame - titleBarHeight.
            expect(content.width).to.equal(bounds.width);
            expect(content.height).to.equal(bounds.height);
          });

          it('keeps the requested content size when using hiddenInset + useContentSize=true', () => {
            const w = new LynxWindow({
              show: false,
              width: 1512,
              height: 870,
              useContentSize: true,
              titleBarStyle: 'hiddenInset',
            });
            const content = w.getContentBounds();
            expect(content.width).to.equal(1512);
            expect(content.height).to.equal(870);
          });
        }
      );

      ifdescribe(process.platform === 'darwin')(
        'LynxWindow trafficLightPosition',
        () => {
          afterEach(closeAllWindows);

          it('stores constructor and setter values', () => {
            const w = new LynxWindow({
              show: false,
              frame: false,
              titleBarStyle: 'hidden',
              trafficLightPosition: { x: 12, y: 14 },
            });
            const getPosition = () =>
              (w as any).getTrafficLightPosition() as { x: number; y: number };
            expect(getPosition()).to.deep.equal({ x: 12, y: 14 });
            w.setTrafficLightPosition({ x: 30, y: 40 });
            expect(getPosition()).to.deep.equal({ x: 30, y: 40 });
            w.setTrafficLightPosition({ x: 0, y: 0 });
            expect(getPosition()).to.deep.equal({ x: 0, y: 0 });
          });
        }
      );

      ifdescribe(process.platform === 'darwin')(
        'fullscreen state with resizable set',
        () => {
          afterEach(closeAllWindows);

          it('resizable flag should be set to false and restored', async () => {
            const w = new LynxWindow({ resizable: false });

            const enterFullScreen = once(w, 'enter-full-screen');
            w.setFullScreen(true);
            await enterFullScreen;
            expect(w.resizable).to.equal(false);

            await setTimeout();
            const leaveFullScreen = once(w, 'leave-full-screen');
            w.setFullScreen(false);
            await leaveFullScreen;
            expect(w.resizable).to.equal(false);
          });

          it('default resizable flag should be restored after entering/exiting fullscreen', async () => {
            const w = new LynxWindow();

            const enterFullScreen = once(w, 'enter-full-screen');
            w.setFullScreen(true);
            await enterFullScreen;
            expect(w.resizable).to.equal(false);

            await setTimeout();
            const leaveFullScreen = once(w, 'leave-full-screen');
            w.setFullScreen(false);
            await leaveFullScreen;
            expect(w.resizable).to.equal(true);
          });
        }
      );

      ifdescribe(process.platform === 'darwin' || process.platform === 'win32')(
        'LynxWindow move events',
        () => {
          afterEach(closeAllWindows);

          it('emits move when the window position changes', async () => {
            const w = new LynxWindow({ show: false });
            const shown = once(w, 'show');
            w.show();
            await shown;

            const [x, y] = w.getPosition();
            const move = once(w, 'move');
            w.setPosition(x + 20, y + 20);
            await move;

            await waitUntil(() => {
              const [nextX, nextY] = w.getPosition();
              return nextX === x + 20 && nextY === y + 20;
            });
          });

          it('emits moved after move when the window position changes', async () => {
            const w = new LynxWindow({ show: false });
            const shown = once(w, 'show');
            w.show();
            await shown;

            const events: string[] = [];
            const move = once(w, 'move').then(() => {
              events.push('move');
            });
            const moved = once(w, 'moved').then(() => {
              events.push('moved');
            });

            const [x, y] = w.getPosition();
            w.setPosition(x + 40, y + 40);

            await move;
            await moved;

            expect(events).to.deep.equal(['move', 'moved']);
          });
        }
      );

      describe('LynxWindow.setAlwaysOnTop(flag, level)', () => {
        let w: LynxWindow;

        beforeEach(() => {
          w = new LynxWindow({ show: false });
        });
        afterEach(async () => {
          await closeWindow(w);
          w = (null as unknown) as LynxWindow;
        });

        it('defaults to false and can be toggled', () => {
          expect(w.isAlwaysOnTop()).to.be.false('is alwaysOnTop');

          w.setAlwaysOnTop(true, 'screen-saver');
          expect(w.isAlwaysOnTop()).to.be.true('is not alwaysOnTop');

          w.setAlwaysOnTop(false);
          expect(w.isAlwaysOnTop()).to.be.false('is alwaysOnTop');

          w.setAlwaysOnTop(true);
          expect(w.isAlwaysOnTop()).to.be.true('is not alwaysOnTop');
        });

        it('honors the alwaysOnTop constructor option', async () => {
          await closeWindow(w, { assertNotWindows: false });
          w = new LynxWindow({ show: false, alwaysOnTop: true });

          expect(w.isAlwaysOnTop()).to.be.true('is not alwaysOnTop');
        });

        it('works when called prior to show', async () => {
          w.setAlwaysOnTop(true, 'screen-saver');

          const shown = once(w, 'show');
          w.show();
          await shown;

          expect(w.isAlwaysOnTop()).to.be.true('is not alwaysOnTop');
        });

        it('works when called prior to showInactive', async () => {
          w.setAlwaysOnTop(true, 'screen-saver');

          const shown = once(w, 'show');
          w.showInactive();
          await shown;

          expect(w.isAlwaysOnTop()).to.be.true('is not alwaysOnTop');
        });

        ifit(process.platform === 'darwin' || process.platform === 'win32')(
          'emits always-on-top-changed with the latest state',
          async () => {
            const changedToTrue = once(w, 'always-on-top-changed');
            w.setAlwaysOnTop(true, 'screen-saver');
            const [, isAlwaysOnTopTrue] = await changedToTrue;
            expect(isAlwaysOnTopTrue).to.equal(true);

            const changedToFalse = once(w, 'always-on-top-changed');
            w.setAlwaysOnTop(false);
            const [, isAlwaysOnTopFalse] = await changedToFalse;
            expect(isAlwaysOnTopFalse).to.equal(false);
          }
        );

        ifit(process.platform === 'darwin')(
          'honors the alwaysOnTop level of a child window',
          async () => {
            const parent = new LynxWindow({ show: false });
            const child = new LynxWindow({ show: false, parent });

            child.setAlwaysOnTop(true, 'screen-saver');

            expect(parent.isAlwaysOnTop()).to.be.false();
            expect(child.isAlwaysOnTop()).to.be.true(
              'child is not always on top'
            );
            expect((child as any)._getAlwaysOnTopLevel()).to.equal(
              'screen-saver'
            );

            await closeWindow(child, { assertNotWindows: false });
            await closeWindow(parent, { assertNotWindows: false });
          }
        );

        ifit(process.platform === 'darwin')(
          'resets the alwaysOnTop state while minimized and restores it afterwards',
          async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            w.setAlwaysOnTop(true, 'screen-saver');
            expect(w.isAlwaysOnTop()).to.be.true('is not alwaysOnTop');

            const minimized = once(w, 'minimize');
            w.minimize();
            await minimized;
            expect(w.isAlwaysOnTop()).to.be.false('is alwaysOnTop');

            const restored = once(w, 'restore');
            w.restore();
            await restored;
            expect(w.isAlwaysOnTop()).to.be.true('is not alwaysOnTop');
          }
        );
      });

      describe('LynxWindow.moveTop()', () => {
        afterEach(closeAllWindows);

        ifit(process.platform !== 'linux')(
          'should not steal focus',
          async () => {
            const posDelta = 50;
            const wShownInactive = once(w, 'show');
            w.showInactive();
            await wShownInactive;
            expect(w.isFocused()).to.equal(false);

            const otherWindow = new LynxWindow({
              show: false,
              title: 'otherWindow',
            });
            const otherWindowShown = once(otherWindow, 'show');
            const otherWindowFocused = once(otherWindow, 'focus');
            otherWindow.show();
            await otherWindowShown;
            await otherWindowFocused;
            expect(otherWindow.isFocused()).to.equal(true);

            w.moveTop();
            const wPos = w.getPosition();
            const wMoving = once(w, 'move');
            w.setPosition(wPos[0] + posDelta, wPos[1] + posDelta);
            await wMoving;
            expect(w.isFocused()).to.equal(false);
            expect(otherWindow.isFocused()).to.equal(true);

            const wFocused = once(w, 'focus');
            const otherWindowBlurred = once(otherWindow, 'blur');
            w.focus();
            await wFocused;
            await otherWindowBlurred;
            expect(w.isFocused()).to.equal(true);

            otherWindow.moveTop();
            const otherWindowPos = otherWindow.getPosition();
            const otherWindowMoving = once(otherWindow, 'move');
            otherWindow.setPosition(
              otherWindowPos[0] + posDelta,
              otherWindowPos[1] + posDelta
            );
            await otherWindowMoving;
            expect(otherWindow.isFocused()).to.equal(false);
            expect(w.isFocused()).to.equal(true);

            await closeWindow(otherWindow, { assertNotWindows: false });
          }
        );

        it('should not crash when called on a modal child window', async () => {
          const shown = once(w, 'show');
          w.show();
          await shown;

          const child = new LynxWindow({ modal: true, parent: w });
          expect(() => {
            child.moveTop();
          }).to.not.throw();
        });
      });

      describe('LynxWindow parent/child windows', () => {
        afterEach(closeAllWindows);

        it('exposes parent and child windows', () => {
          const parent = new LynxWindow({ show: false });
          const child = new LynxWindow({ show: false, parent });

          expect(child.getParentWindow()).to.equal(parent);
          expect(parent.getChildWindows()).to.include(child);

          child.setParentWindow(null);

          expect(child.getParentWindow()).to.equal(null);
          expect(parent.getChildWindows()).to.not.include(child);
        });
      });

      ifdescribe(process.platform === 'darwin')(
        'LynxWindow modal sheets',
        () => {
          afterEach(closeAllWindows);

          it('does not close a modal sheet when re-enabling the parent window', async () => {
            const parent = new LynxWindow({ show: false });
            const parentShow = once(parent, 'show');
            parent.show();
            await parentShow;

            const child = new LynxWindow({
              show: false,
              parent,
              modal: true,
            });

            const sheetBegin = once(parent, 'sheet-begin');
            child.show();
            await sheetBegin;
            await waitUntil(() => !parent.isEnabled());

            parent.setEnabled(true);

            await setTimeout(50);
            expect(child.isVisible()).to.equal(true);
            expect(parent.isEnabled()).to.equal(false);
          });

          it('emits sheet-end when a modal sheet is closed', async () => {
            const parent = new LynxWindow({ show: false });
            const parentShow = once(parent, 'show');
            parent.show();
            await parentShow;

            const child = new LynxWindow({
              show: false,
              parent,
              modal: true,
            });

            const sheetBegin = once(parent, 'sheet-begin');
            child.show();
            await sheetBegin;

            const sheetEnd = once(parent, 'sheet-end');
            child.close();
            await sheetEnd;
          });
        }
      );

      ifdescribe(process.platform === 'win32' || process.platform === 'darwin')(
        'LynxWindow modal parent enable state',
        () => {
          afterEach(closeAllWindows);

          it('disables parent while modal child is visible', async () => {
            const parent = new LynxWindow({ show: false });
            const parentShown = once(parent, 'show');
            parent.show();
            await parentShown;

            expect(parent.isEnabled()).to.equal(true);

            const child = new LynxWindow({
              show: false,
              parent,
              modal: true,
            });

            if (process.platform === 'darwin') {
              const sheetBegin = once(parent, 'sheet-begin');
              child.show();
              await sheetBegin;
            } else {
              const childShown = once(child, 'show');
              child.show();
              await childShown;
            }
            await waitUntil(() => parent.isEnabled() === false);
            expect(parent.isEnabled()).to.equal(false);

            child.hide();
            await waitUntil(() => parent.isEnabled() === true);
            expect(parent.isEnabled()).to.equal(true);
          });

          it('defaults minimizable to false for modal child window', () => {
            const parent = new LynxWindow({ show: false });
            const child = new LynxWindow({ show: false, parent, modal: true });
            expect(child.minimizable).to.equal(false);
          });

          it('does not override explicit minimizable for modal child window', () => {
            const parent = new LynxWindow({ show: false });
            const child = new LynxWindow({
              show: false,
              parent,
              modal: true,
              minimizable: true,
            });
            expect(child.minimizable).to.equal(true);
          });
        }
      );

      describe('LynxWindow.setFocusable()', () => {
        it('can set unfocusable window to focusable', async () => {
          const w2 = new LynxWindow({ focusable: false });
          const w2Focused = once(w2, 'focus');
          w2.setFocusable(true);
          w2.focus();
          await w2Focused;
          await closeWindow(w2, { assertNotWindows: false });
        });
      });

      describe('LynxWindow.isFocusable()', () => {
        ifit(process.platform === 'win32' || process.platform === 'darwin')(
          'correctly returns whether a window is focusable',
          async () => {
            const w2 = new LynxWindow({ focusable: false });
            try {
              expect(w2.isFocusable()).to.be.false();

              w2.setFocusable(true);
              expect(w2.isFocusable()).to.be.true();
            } finally {
              await closeWindow(w2, { assertNotWindows: false });
            }
          }
        );
      });

      describe('LynxWindow focusable property', () => {
        ifit(process.platform === 'win32' || process.platform === 'darwin')(
          'can be changed through the focusable property',
          async () => {
            const w2 = new LynxWindow({ show: false });
            try {
              expect(w2.focusable).to.be.true();
              w2.focusable = false;
              expect(w2.focusable).to.be.false();
              expect(w2.isFocusable()).to.be.false();
              w2.focusable = true;
              expect(w2.focusable).to.be.true();
            } finally {
              await closeWindow(w2, { assertNotWindows: false });
            }
          }
        );
      });

      ifdescribe(process.platform === 'darwin')(
        'LynxWindow.getNativeWindowHandle()',
        () => {
          it('returns a unique native handle buffer per window', async () => {
            const w1 = new LynxWindow({ show: false });
            const w2 = new LynxWindow({ show: false });

            try {
              const handle1 = w1.getNativeWindowHandle();
              const handle2 = w2.getNativeWindowHandle();

              // Follow Electron-style expectations: the handle is exposed as
              // a raw pointer-sized Buffer and different windows should not
              // share the same native view handle.
              expect(Buffer.isBuffer(handle1)).to.equal(true);
              expect(Buffer.isBuffer(handle2)).to.equal(true);
              expect(handle1.length).to.equal(expectedNativeHandleSize);
              expect(handle2.length).to.equal(expectedNativeHandleSize);
              expect(handle1.equals(Buffer.alloc(handle1.length, 0))).to.equal(
                false
              );
              expect(handle2.equals(Buffer.alloc(handle2.length, 0))).to.equal(
                false
              );
              expect(handle1.equals(handle2)).to.equal(false);
            } finally {
              await closeWindow(w1, { assertNotWindows: false });
              await closeWindow(w2, { assertNotWindows: false });
            }
          });
        }
      );

      describe('BaseWindow.fromId(id)', () => {
        afterEach(closeAllWindows);

        it('returns the window with id', () => {
          const w = new LynxWindow({ show: false });
          expect(BaseWindow.fromId(w.id)).to.equal(w);
        });
      });

      describe('LynxWindow.setBounds(bounds[, animate])', () => {
        it('sets the window bounds with full bounds', async () => {
          const bounds = w.getBounds();
          const fullBounds = {
            x: bounds.x + 20,
            y: bounds.y + 20,
            width: bounds.width + 40,
            height: bounds.height + 30,
          };
          const resize = once(w, 'resize');
          w.setBounds(fullBounds);
          await resize;
          expectBoundsEqual(w.getBounds(), fullBounds);
        });

        it('sets the window bounds with partial bounds', async () => {
          const bounds = w.getBounds();
          const fullBounds = {
            x: bounds.x + 10,
            y: bounds.y + 10,
            width: bounds.width + 50,
            height: bounds.height + 20,
          };
          const resize = once(w, 'resize');
          w.setBounds(fullBounds);
          await resize;

          const boundsUpdate = { width: fullBounds.width - 25 };
          const resizeUpdated = once(w, 'resize');
          w.setBounds(boundsUpdate);
          await resizeUpdated;

          expectBoundsEqual(w.getBounds(), { ...fullBounds, ...boundsUpdate });
        });

        ifit(process.platform === 'darwin')(
          'emits resized event after animating',
          async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            const bounds = w.getBounds();
            const resized = once(w, 'resized');
            w.setBounds(
              {
                ...bounds,
                width: bounds.width + 20,
                height: bounds.height + 20,
              },
              true
            );
            await resized;
          }
        );

        it('does not emit the resize event for move-only changes', async () => {
          const [x, y] = w.getPosition();
          let resizeEmitted = false;
          w.once('resize', () => {
            resizeEmitted = true;
          });

          w.setBounds({ x: x + 10, y: y + 10 });
          await setTimeout(100);

          expect(resizeEmitted).to.equal(false);
        });
      });

      describe('LynxWindow.setMinimum/MaximumSize(width, height)', () => {
        it('sets the maximum and minimum size of the window', () => {
          expect(w.getMinimumSize()).to.deep.equal([0, 0]);
          expect(w.getMaximumSize()).to.deep.equal([0, 0]);

          w.setMinimumSize(100, 100);
          expectBoundsEqual(w.getMinimumSize(), [100, 100]);
          expectBoundsEqual(w.getMaximumSize(), [0, 0]);

          w.setMaximumSize(900, 600);
          expectBoundsEqual(w.getMinimumSize(), [100, 100]);
          expectBoundsEqual(w.getMaximumSize(), [900, 600]);
        });

        it('clamps the current window size when minimum size increases', async () => {
          const resize = once(w, 'resize');
          w.setSize(200, 200);
          await resize;

          w.setMinimumSize(300, 320);
          await waitUntil(() => {
            const [width, height] = w.getSize();
            return width >= 300 && height >= 320;
          });

          const [width, height] = w.getSize();
          expect(width).to.be.at.least(300);
          expect(height).to.be.at.least(320);
        });

        it('clamps the current window size when maximum size decreases', async () => {
          const resize = once(w, 'resize');
          w.setSize(500, 480);
          await resize;

          w.setMaximumSize(260, 240);
          await waitUntil(() => {
            const [width, height] = w.getSize();
            return width <= 260 && height <= 240;
          });

          const [width, height] = w.getSize();
          expect(width).to.be.at.most(260);
          expect(height).to.be.at.most(240);
        });
      });

      describe('LynxWindow.center()', () => {
        it('moves the window away from a custom position', async () => {
          const w2 = new LynxWindow({
            show: false,
            x: 10,
            y: 10,
            width: 200,
            height: 200,
          });

          try {
            const before = w2.getPosition();
            w2.center();
            await waitUntil(() => {
              const after = w2.getPosition();
              return after[0] !== before[0] || after[1] !== before[1];
            });
          } finally {
            await closeWindow(w2, { assertNotWindows: false });
          }
        });

        ifit(process.platform === 'win32')(
          'keeps the DIP size when centering on Windows',
          async () => {
            const w2 = new LynxWindow({
              show: false,
              x: 10,
              y: 10,
              width: 320,
              height: 240,
            });

            try {
              expectBoundsEqual(w2.getSize(), [320, 240]);

              const beforePosition = w2.getPosition();
              w2.center();
              await waitUntil(() => {
                const afterPosition = w2.getPosition();
                return (
                  afterPosition[0] !== beforePosition[0] ||
                  afterPosition[1] !== beforePosition[1]
                );
              });

              expectBoundsEqual(w2.getSize(), [320, 240]);
              const bounds = w2.getBounds();
              expectBoundsEqual([bounds.width, bounds.height], [320, 240]);
            } finally {
              await closeWindow(w2, { assertNotWindows: false });
            }
          }
        );
      });

      describe('LynxWindow.setAspectRatio(ratio)', () => {
        it('resets the behavior when passing in 0', async () => {
          const size = [300, 400];
          w.setAspectRatio(1 / 2);
          w.setAspectRatio(0);
          const resize = once(w, 'resize');
          w.setSize(size[0], size[1]);
          await resize;
          expectBoundsEqual(w.getSize(), size);
        });

        it('does not change bounds when maximum size is set', () => {
          w.setMaximumSize(400, 400);
          w.setAspectRatio(1.0);
          expectBoundsEqual(w.getSize(), [400, 400]);
        });
      });

      describe('LynxWindow.getNormalBounds()', () => {
        it('checks normal bounds after resize', async () => {
          const size = [300, 400];
          const resize = once(w, 'resize');
          w.setSize(size[0], size[1]);
          await resize;
          expectBoundsEqual(w.getNormalBounds(), w.getBounds());
        });

        it('checks normal bounds after move', async () => {
          const [x, y] = w.getPosition();
          const move = once(w, 'move');
          w.setPosition(x + 10, y + 10);
          await move;
          expectBoundsEqual(w.getNormalBounds(), w.getBounds());
        });

        ifit(process.platform !== 'linux')(
          'checks normal bounds when maximized',
          async () => {
            const bounds = w.getBounds();
            const shown = once(w, 'show');
            w.show();
            await shown;

            const maximize = once(w, 'maximize');
            w.maximize();
            await maximize;
            expectBoundsEqual(w.getNormalBounds(), bounds);
          }
        );

        ifit(process.platform === 'win32')(
          'returns the normal size in DIP on Windows',
          async () => {
            const w2 = new LynxWindow({
              show: true,
              width: 320,
              height: 240,
            });

            try {
              await waitUntil(() => w2.isVisible());

              const originalBounds = w2.getBounds();
              expectBoundsEqual(
                [originalBounds.width, originalBounds.height],
                [320, 240]
              );

              const maximize = once(w2, 'maximize');
              w2.maximize();
              await maximize;

              const normalBounds = w2.getNormalBounds();
              expectBoundsEqual(
                [normalBounds.width, normalBounds.height],
                [320, 240]
              );
            } finally {
              await closeWindow(w2, { assertNotWindows: false });
            }
          }
        );

        ifit(process.platform !== 'linux')(
          'updates normal bounds after move and maximize',
          async () => {
            const [x, y] = w.getPosition();
            const move = once(w, 'move');
            w.setPosition(x + 10, y + 10);
            await move;
            const original = w.getBounds();

            const shown = once(w, 'show');
            w.show();
            await shown;

            const maximize = once(w, 'maximize');
            w.maximize();
            await maximize;

            const normal = w.getNormalBounds();
            const bounds = w.getBounds();

            expect(normal).to.deep.equal(original);
            expect(normal).to.not.deep.equal(bounds);
          }
        );

        ifit(process.platform !== 'linux')(
          'checks normal bounds when unmaximized',
          async () => {
            const bounds = w.getBounds();
            const shown = once(w, 'show');
            w.show();
            await shown;

            const maximize = once(w, 'maximize');
            w.maximize();
            await maximize;

            const unmaximize = once(w, 'unmaximize');
            w.unmaximize();
            await unmaximize;
            expectBoundsEqual(w.getNormalBounds(), bounds);
            expect(w.isMaximized()).to.equal(false);
          }
        );

        ifit(process.platform !== 'linux')(
          'updates normal bounds after resize and minimize',
          async () => {
            const resize = once(w, 'resize');
            w.setSize(300, 400);
            await resize;
            const original = w.getBounds();

            const shown = once(w, 'show');
            w.show();
            await shown;

            const minimized = once(w, 'minimize');
            w.minimize();
            await minimized;

            const normal = w.getNormalBounds();

            expect(normal).to.deep.equal(original);
            expectBoundsEqual(normal, w.getBounds());
          }
        );

        ifit(process.platform !== 'linux')(
          'updates normal bounds after move and minimize',
          async () => {
            const [x, y] = w.getPosition();
            const move = once(w, 'move');
            w.setPosition(x + 10, y + 10);
            await move;
            const original = w.getBounds();

            const shown = once(w, 'show');
            w.show();
            await shown;

            const minimized = once(w, 'minimize');
            w.minimize();
            await minimized;

            const normal = w.getNormalBounds();

            expect(normal).to.deep.equal(original);
            expectBoundsEqual(normal, w.getBounds());
          }
        );

        ifit(process.platform !== 'linux')(
          'checks normal bounds when minimized',
          async () => {
            const bounds = w.getBounds();
            const shown = once(w, 'show');
            w.show();
            await shown;

            const minimized = once(w, 'minimize');
            w.minimize();
            await minimized;

            expectBoundsEqual(w.getNormalBounds(), bounds);
          }
        );

        ifit(process.platform !== 'linux')(
          'checks normal bounds when restored',
          async () => {
            const bounds = w.getBounds();
            const shown = once(w, 'show');
            w.show();
            await shown;

            w.once('minimize', () => {
              w.restore();
            });
            const restored = once(w, 'restore');
            w.minimize();
            await restored;

            expectBoundsEqual(w.getNormalBounds(), bounds);
          }
        );
      });

      describe('LynxWindow.isNormal()', () => {
        it('returns true for a newly created window', () => {
          expect(w.isNormal()).to.equal(true);
        });

        ifit(process.platform !== 'linux')(
          'returns false when maximized and true after restore',
          async () => {
            const shown = once(w, 'show');
            w.show();
            await shown;

            const maximize = once(w, 'maximize');
            w.maximize();
            await maximize;
            expect(w.isNormal()).to.equal(false);

            const restore = once(w, 'restore');
            w.restore();
            await restore;
            expect(w.isNormal()).to.equal(true);
          }
        );
      });

      ifdescribe(process.platform !== 'linux')(
        'LynxWindow.setOpacity(opacity)',
        () => {
          afterEach(closeAllWindows);

          it('makes a window with initial opacity', () => {
            const w = new LynxWindow({ show: false, opacity: 0.5 });
            expect(w.getOpacity()).to.equal(0.5);
          });

          it('allows setting the opacity', () => {
            const w = new LynxWindow({ show: false });
            expect(() => {
              w.setOpacity(0.0);
              expect(w.getOpacity()).to.equal(0.0);
              w.setOpacity(0.5);
              expect(w.getOpacity()).to.equal(0.5);
              w.setOpacity(1.0);
              expect(w.getOpacity()).to.equal(1.0);
            }).to.not.throw();
          });

          it('clamps opacity to [0.0...1.0]', () => {
            const w = new LynxWindow({ show: false, opacity: 0.5 });
            w.setOpacity(100);
            expect(w.getOpacity()).to.equal(1.0);
            w.setOpacity(-100);
            expect(w.getOpacity()).to.equal(0.0);
          });
        }
      );

      ifdescribe(process.platform === 'darwin')(
        'visibleOnAllWorkspaces state',
        () => {
          afterEach(closeAllWindows);

          describe('with properties', () => {
            it('can be changed', () => {
              const w = new LynxWindow({ show: false });
              expect(w.visibleOnAllWorkspaces).to.be.false();
              w.visibleOnAllWorkspaces = true;
              expect(w.visibleOnAllWorkspaces).to.be.true();
            });
          });

          describe('with functions', () => {
            it('can be changed', () => {
              const w = new LynxWindow({ show: false });
              expect(w.isVisibleOnAllWorkspaces()).to.be.false();
              w.setVisibleOnAllWorkspaces(true);
              expect(w.isVisibleOnAllWorkspaces()).to.be.true();
            });
          });
        }
      );

      describe('native window title', () => {
        afterEach(closeAllWindows);

        describe('with properties', () => {
          it('can be set with title constructor option', () => {
            const w = new LynxWindow({ show: false, title: 'mYtItLe' });
            expect(w.title).to.equal('mYtItLe');
          });

          it('can be changed', () => {
            const w = new LynxWindow({ show: false });
            w.title = 'NEW TITLE';
            expect(w.title).to.equal('NEW TITLE');
          });
        });

        describe('with functions', () => {
          it('can be set with title constructor option', () => {
            const w = new LynxWindow({ show: false, title: 'mYtItLe' });
            expect(w.getTitle()).to.equal('mYtItLe');
          });

          it('can be changed', () => {
            const w = new LynxWindow({ show: false });
            w.setTitle('NEW TITLE');
            expect(w.getTitle()).to.equal('NEW TITLE');
          });
        });
      });

      describe('hasShadow state', () => {
        afterEach(closeAllWindows);

        describe('with properties', () => {
          it('returns a boolean on all platforms', () => {
            const w = new LynxWindow({ show: false });
            expect(w.shadow).to.be.a('boolean');
          });

          it('can be changed with hasShadow option', () => {
            const w = new LynxWindow({ show: false, hasShadow: false });
            expect(w.shadow).to.equal(false);
          });

          it('can be changed through the shadow property', () => {
            const w = new LynxWindow({ show: false });
            w.shadow = false;
            expect(w.shadow).to.be.false('shadow');
            w.shadow = true;
            expect(w.shadow).to.be.true('shadow');
          });
        });

        describe('with functions', () => {
          it('can be changed with setHasShadow method', () => {
            const w = new LynxWindow({ show: false });
            w.setHasShadow(false);
            expect(w.hasShadow()).to.be.false('hasShadow');
            w.setHasShadow(true);
            expect(w.hasShadow()).to.be.true('hasShadow');
          });
        });
      });

      describe('LynxWindow.isModal()', () => {
        afterEach(closeAllWindows);

        it('returns false for a regular window', () => {
          const regular = new LynxWindow({ show: false });
          expect(regular.isModal()).to.equal(false);
        });

        it('returns true for a modal child window', () => {
          const parent = new LynxWindow({ show: false });
          const child = new LynxWindow({
            show: false,
            parent,
            modal: true,
          });

          expect(child.isModal()).to.equal(true);
          expect(parent.isModal()).to.equal(false);
        });
      });

      ifdescribe(process.platform === 'darwin' || process.platform === 'win32')(
        'window states',
        () => {
          afterEach(closeAllWindows);

          describe('movable state', () => {
            describe('with properties', () => {
              it('can be set with movable constructor option', () => {
                const w = new LynxWindow({ show: false, movable: false });
                expect(w.movable).to.be.false('movable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.movable).to.be.true('movable');
                w.movable = false;
                expect(w.movable).to.be.false('movable');
                w.movable = true;
                expect(w.movable).to.be.true('movable');
              });
            });

            describe('with functions', () => {
              it('can be set with movable constructor option', () => {
                const w = new LynxWindow({ show: false, movable: false });
                expect(w.isMovable()).to.be.false('isMovable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.isMovable()).to.be.true('isMovable');
                w.setMovable(false);
                expect(w.isMovable()).to.be.false('isMovable');
                w.setMovable(true);
                expect(w.isMovable()).to.be.true('isMovable');
              });
            });
          });

          describe('minimizable state', () => {
            describe('with properties', () => {
              it('can be set with minimizable constructor option', () => {
                const w = new LynxWindow({ show: false, minimizable: false });
                expect(w.minimizable).to.be.false('minimizable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.minimizable).to.be.true('minimizable');
                w.minimizable = false;
                expect(w.minimizable).to.be.false('minimizable');
                w.minimizable = true;
                expect(w.minimizable).to.be.true('minimizable');
              });
            });

            describe('with functions', () => {
              it('can be set with minimizable constructor option', () => {
                const w = new LynxWindow({ show: false, minimizable: false });
                expect(w.isMinimizable()).to.be.false('isMinimizable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.isMinimizable()).to.be.true('isMinimizable');
                w.setMinimizable(false);
                expect(w.isMinimizable()).to.be.false('isMinimizable');
                w.setMinimizable(true);
                expect(w.isMinimizable()).to.be.true('isMinimizable');
              });
            });
          });

          describe('maximizable state', () => {
            describe('with properties', () => {
              it('can be set with maximizable constructor option', () => {
                const w = new LynxWindow({ show: false, maximizable: false });
                expect(w.maximizable).to.be.false('maximizable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.maximizable).to.be.true('maximizable');
                w.maximizable = false;
                expect(w.maximizable).to.be.false('maximizable');
                w.maximizable = true;
                expect(w.maximizable).to.be.true('maximizable');
              });
            });

            describe('with functions', () => {
              it('can be set with maximizable constructor option', () => {
                const w = new LynxWindow({ show: false, maximizable: false });
                expect(w.isMaximizable()).to.be.false('isMaximizable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.isMaximizable()).to.be.true('isMaximizable');
                w.setMaximizable(false);
                expect(w.isMaximizable()).to.be.false('isMaximizable');
                w.setMaximizable(true);
                expect(w.isMaximizable()).to.be.true('isMaximizable');
              });
            });
          });

          describe('resizable state', () => {
            describe('with properties', () => {
              it('can be set with resizable constructor option', () => {
                const w = new LynxWindow({ show: false, resizable: false });
                expect(w.resizable).to.be.false('resizable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.resizable).to.be.true('resizable');
                w.resizable = false;
                expect(w.resizable).to.be.false('resizable');
                w.resizable = true;
                expect(w.resizable).to.be.true('resizable');
              });
            });

            describe('with functions', () => {
              it('can be set with resizable constructor option', () => {
                const w = new LynxWindow({ show: false, resizable: false });
                expect(w.isResizable()).to.be.false('isResizable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.isResizable()).to.be.true('isResizable');
                w.setResizable(false);
                expect(w.isResizable()).to.be.false('isResizable');
                w.setResizable(true);
                expect(w.isResizable()).to.be.true('isResizable');
              });
            });
          });

          describe('closable state', () => {
            describe('with properties', () => {
              it('can be set with closable constructor option', () => {
                const w = new LynxWindow({ show: false, closable: false });
                expect(w.closable).to.be.false('closable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.closable).to.be.true('closable');
                w.closable = false;
                expect(w.closable).to.be.false('closable');
                w.closable = true;
                expect(w.closable).to.be.true('closable');
              });
            });

            describe('with functions', () => {
              it('can be set with closable constructor option', () => {
                const w = new LynxWindow({ show: false, closable: false });
                expect(w.isClosable()).to.be.false('isClosable');
              });

              it('can be changed', () => {
                const w = new LynxWindow({ show: false });
                expect(w.isClosable()).to.be.true('isClosable');
                w.setClosable(false);
                expect(w.isClosable()).to.be.false('isClosable');
                w.setClosable(true);
                expect(w.isClosable()).to.be.true('isClosable');
              });
            });
          });
        }
      );

      ifdescribe(process.platform === 'win32')('autoHideMenuBar state', () => {
        afterEach(closeAllWindows);

        describe('with properties', () => {
          it('can be set with autoHideMenuBar constructor option', () => {
            const w = new LynxWindow({ show: false, autoHideMenuBar: true });
            expect(w.autoHideMenuBar).to.be.true('autoHideMenuBar');
          });

          it('can be changed', () => {
            const w = new LynxWindow({ show: false });
            expect(w.autoHideMenuBar).to.be.false('autoHideMenuBar');
            w.autoHideMenuBar = true;
            expect(w.autoHideMenuBar).to.be.true('autoHideMenuBar');
            w.autoHideMenuBar = false;
            expect(w.autoHideMenuBar).to.be.false('autoHideMenuBar');
          });
        });

        describe('with functions', () => {
          it('can be set with autoHideMenuBar constructor option', () => {
            const w = new LynxWindow({ show: false, autoHideMenuBar: true });
            expect(w.isMenuBarAutoHide()).to.be.true('autoHideMenuBar');
          });

          it('can be changed', () => {
            const w = new LynxWindow({ show: false });
            expect(w.isMenuBarAutoHide()).to.be.false('autoHideMenuBar');
            w.setAutoHideMenuBar(true);
            expect(w.isMenuBarAutoHide()).to.be.true('autoHideMenuBar');
            w.setAutoHideMenuBar(false);
            expect(w.isMenuBarAutoHide()).to.be.false('autoHideMenuBar');
          });
        });
      });

      describe('LynxWindow content size', () => {
        it('sets the content size', async () => {
          const size = [456, 567];
          const resize = once(w, 'resize');
          w.setContentSize(size[0], size[1]);
          await resize;
          const after = w.getContentSize();
          expect(after).to.deep.equal(size);
        });

        it('gets the content size', () => {
          const size = w.getContentSize();
          expect(size).to.be.an('array');
          expect(size).to.have.lengthOf(2);
          expect(size[0]).to.be.a('number');
          expect(size[1]).to.be.a('number');
        });

        it('sets the content size with animation', async () => {
          const size = [500, 500];
          const resize = once(w, 'resize');
          w.setContentSize(size[0], size[1], true);
          await resize;
          const after = w.getContentSize();
          expect(after).to.deep.equal(size);
        });
      });

      describe('LynxWindow content bounds', () => {
        it('sets the content size and position', async () => {
          const bounds = { x: 10, y: 10, width: 250, height: 250 };
          const resize = once(w, 'resize');
          w.setContentBounds(bounds);
          await resize;
          await setTimeout();
          const result = w.getContentBounds();
          // Check width and height match
          expect(result.width).to.equal(bounds.width);
          expect(result.height).to.equal(bounds.height);
          // Position might be adjusted by the system to keep window visible
          expect(result.x).to.be.a('number');
          expect(result.y).to.be.a('number');
        });

        it('gets the content bounds', () => {
          const bounds = w.getContentBounds();
          expect(bounds).to.have.property('x');
          expect(bounds).to.have.property('y');
          expect(bounds).to.have.property('width');
          expect(bounds).to.have.property('height');
          expect(bounds.x).to.be.a('number');
          expect(bounds.y).to.be.a('number');
          expect(bounds.width).to.be.a('number');
          expect(bounds.height).to.be.a('number');
        });

        it('sets the content bounds with animation', async () => {
          const bounds = { x: 20, y: 20, width: 300, height: 300 };
          const resize = once(w, 'resize');
          w.setContentBounds(bounds, true);
          await resize;
          const result = w.getContentBounds();
          // Check width and height match
          expect(result.width).to.equal(bounds.width);
          expect(result.height).to.equal(bounds.height);
          // Position might be adjusted by the system to keep window visible
          expect(result.x).to.be.a('number');
          expect(result.y).to.be.a('number');
        });

        it('content size respects window frame', async () => {
          const wWithFrame = new LynxWindow({
            show: false,
            frame: true,
            width: 400,
            height: 300,
          });
          const contentSize = wWithFrame.getContentSize();
          const windowSize = wWithFrame.getSize();
          // Content size should be smaller than window size when frame is present
          expect(contentSize[0]).to.be.at.most(windowSize[0]);
          expect(contentSize[1]).to.be.at.most(windowSize[1]);
          if (process.platform === 'darwin') {
            expect(contentSize[1]).to.be.lessThan(windowSize[1]);
          }
          await closeWindow(wWithFrame, { assertNotWindows: false });
        });

        it('content size equals window size for frameless window', async () => {
          const wFrameless = new LynxWindow({
            show: false,
            frame: false,
            width: 400,
            height: 300,
          });
          const contentSize = wFrameless.getContentSize();
          const windowSize = wFrameless.getSize();
          // Content size should equal window size for frameless windows
          expect(contentSize[0]).to.equal(windowSize[0]);
          expect(contentSize[1]).to.equal(windowSize[1]);
          await closeWindow(wFrameless, { assertNotWindows: false });
        });

        describe('LynxWindow will-resize event', () => {
          it('does not emit on programmatic resize', async () => {
            const w2 = new LynxWindow({ show: false, width: 300, height: 200 });

            w2.once('will-resize', () => {
              expect.fail('will-resize event should not be emitted');
            });

            const resize = once(w2, 'resize');
            w2.setSize(320, 220);
            await resize;
            await closeWindow(w2, { assertNotWindows: false });
          });
        });

        ifit(process.platform !== 'linux')(
          'frameless window keeps client size across activation changes',
          async () => {
            const wFrameless = new LynxWindow({
              show: true,
              frame: false,
              resizable: true,
              width: 400,
              height: 300,
              ...(process.platform === 'win32' ? { thickFrame: true } : {}),
            });
            const wOther = new LynxWindow({
              show: true,
              frame: true,
              resizable: false,
              width: 200,
              height: 200,
            });

            wFrameless.focus();
            await waitUntil(() => wFrameless.isFocused());

            const beforeContentSize = wFrameless.getContentSize();
            const beforeWindowSize = wFrameless.getSize();
            expect(beforeContentSize[0]).to.equal(beforeWindowSize[0]);
            expect(beforeContentSize[1]).to.equal(beforeWindowSize[1]);

            wOther.focus();
            await waitUntil(() => wOther.isFocused());
            expect(wFrameless.isFocused()).to.equal(false);

            const afterContentSize = wFrameless.getContentSize();
            const afterWindowSize = wFrameless.getSize();
            expect(afterContentSize[0]).to.equal(afterWindowSize[0]);
            expect(afterContentSize[1]).to.equal(afterWindowSize[1]);

            await closeWindow(wOther, { assertNotWindows: false });
            await closeWindow(wFrameless, { assertNotWindows: false });
          }
        );
      });
    });
  });
});
