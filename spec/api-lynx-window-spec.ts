import { app, LynxWindow } from 'lynxtron';

import { expect } from 'chai';

// @ts-expect-error
import * as childProcess from 'node:child_process';
import { once } from 'node:events';
// import * as fs from 'node:fs';
// import * as http from 'node:http';
// import { AddressInfo } from 'node:net';
// import * as os from 'node:os';
// import * as path from 'node:path';
// import * as qs from 'node:querystring';
// import { setTimeout as syncSetTimeout } from 'node:timers';
import { setTimeout } from 'node:timers/promises';
// import * as nodeUrl from 'node:url';

// import { emittedUntil, emittedNTimes } from './lib/events-helpers';
// import { randomString } from './lib/net-helpers';
// import { HexColors, hasCapturableScreen, ScreenCapture } from './lib/screen-helpers';

// @ts-expect-error
import { ifit, ifdescribe, defer, listen, waitUntil } from './lib/spec-helpers';
import { closeWindow, closeAllWindows } from './lib/window-helpers';

// const fixtures = path.resolve(__dirname, 'fixtures');
// const mainFixtures = path.resolve(__dirname, 'fixtures');

//import { setTimeout as syncSetTimeout } from 'node:timers';

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

    // TODO(Guo Xi): support icon
    // it('does not crash or throw when passed an invalid icon', async () => {
    //   expect(() => {
    //     const w = new LynxWindow({
    //       icon: undefined
    //     } as any);
    //     w.destroy();
    //   }).not.to.throw();
    // });
  });

  describe('garbage collection', () => {
    const v8Util = process._linkedBinding('electron_common_v8_util');
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
      // TODO(Guo Xi): support dialog
      // dialog.showMessageBox(w, { message: 'Hello Error' });
      w.close();
      await closed;
    });

    it('should work if called when multiple messageBoxes are showing', async () => {
      const closed = once(w, 'closed');
      // TODO(Guo Xi): support dialog
      // dialog.showMessageBox(w, { message: 'Hello Error' });
      // dialog.showMessageBox(w, { message: 'Hello Error' });
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

    // it('should emit unload handler', async () => {
    //   await w.loadFile(path.join(fixtures, 'api', 'unload.html'));
    //   const closed = once(w, 'closed');
    //   w.close();
    //   await closed;
    //   const test = path.join(fixtures, 'api', 'unload');
    //   const content = fs.readFileSync(test);
    //   fs.unlinkSync(test);
    //   expect(String(content)).to.equal('unload');
    // });
  });

  // TODO(Guo Xi): LynxWindow load 和 unload 需要 event 吗
  // describe('window.close()', () => {
  //   let w: LynxWindow;
  //   beforeEach(() => {
  //     w = new LynxWindow({ show: false, webPreferences: { nodeIntegration: true, contextIsolation: false } });
  //   });
  //   afterEach(async () => {
  //     await closeWindow(w);
  //     w = null as unknown as LynxWindow;
  //   });

  //   it('should emit unload event', async () => {
  //     w.loadFile(path.join(fixtures, 'api', 'close.html'));
  //     await once(w, 'closed');
  //     const test = path.join(fixtures, 'api', 'close');
  //     const content = fs.readFileSync(test).toString();
  //     fs.unlinkSync(test);
  //     expect(content).to.equal('close');
  //   });

  //   it('should emit beforeunload event', async function () {
  //     await w.loadFile(path.join(__dirname, 'fixtures', 'api', 'beforeunload-false.html'));
  //     w.webContents.executeJavaScript('window.close()', true);
  //     await once(w.webContents, '-before-unload-fired');
  //   });
  // });

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

    // TODO(Guo Xi): LynxWindow.getContentProtection not suported
    // TODO(Guo Xi): LynxWindow.loadURL(url) add loadFile for LynxWindow

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
        // ifit(process.platform !== 'win32')('focuses a blurred window', async () => {
        //   {
        //     const isBlurred = once(w, 'blur');
        //     const isShown = once(w, 'show');
        //     w.show();
        //     w.blur();
        //     await isShown;
        //     await isBlurred;
        //   }
        //   expect(w.isFocused()).to.equal(false);
        //   w.focus();
        //   expect(w.isFocused()).to.equal(true);
        // });

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

        // TODO(Guo Xi): support macOS panel
        // ifit(process.platform === 'darwin')('it does not activate the app if focusing an inactive panel', async () => {
        //   // Show to focus app, then remove existing window
        //   w.show();
        //   w.destroy();

        //   // We first need to resign app focus for this test to work
        //   const isInactive = once(app, 'did-resign-active');
        //   childProcess.execSync('osascript -e \'tell application "Finder" to activate\'');
        //   defer(() => childProcess.execSync('osascript -e \'tell application "Finder" to quit\''));
        //   await isInactive;

        //   // Create new window
        //   w = new LynxWindow({
        //     type: 'panel',
        //     height: 200,
        //     width: 200,
        //     center: true,
        //     show: false
        //   });

        //   const isShow = once(w, 'show');
        //   const isFocus = once(w, 'focus');

        //   w.show();
        //   w.focus();

        //   await isShow;
        //   await isFocus;

        //   const getActiveAppOsa = 'tell application "System Events" to get the name of the first process whose frontmost is true';
        //   const activeApp = childProcess.execSync(`osascript -e '${getActiveAppOsa}'`).toString().trim();

        //   expect(activeApp).to.equal('Finder');
        // });
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

        ifit(process.platform !== 'linux')(
          'transfers focus status to the next window',
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
          }
        );
      });

      describe('LynxWindow.moveTop()', () => {
        afterEach(closeAllWindows);

        it.skip('should not steal focus', async () => {
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
          expect(LynxWindow.getAllWindows()).to.have.lengthOf(1);
        });

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

      describe('LynxWindow.moveAbove(mediaSourceId)', () => {
        it.skip('should throw an exception if wrong formatting', async () => {
          const fakeSourceIds = [
            'none',
            'screen:0',
            'window:fake',
            'window:1234',
            'foobar:1:2',
          ];
          for (const sourceId of fakeSourceIds) {
            expect(() => {
              w.moveAbove(sourceId);
            }).to.throw(/Invalid media source id/);
          }
        });

        it.skip('should throw an exception if wrong type', async () => {
          const fakeSourceIds = [null as any, 123 as any];
          for (const sourceId of fakeSourceIds) {
            expect(() => {
              w.moveAbove(sourceId);
            }).to.throw(/Error processing argument at index 0 */);
          }
        });

        it.skip('should throw an exception if invalid window', async () => {
          // It is very unlikely that these window id exist.
          const fakeSourceIds = [
            'window:99999999:0',
            'window:123456:1',
            'window:123456:9',
          ];
          for (const sourceId of fakeSourceIds) {
            expect(() => {
              w.moveAbove(sourceId);
            }).to.throw(/Invalid media source id/);
          }
        });

        it.skip('should not throw an exception', async () => {
          const w2 = new LynxWindow({ show: false, title: 'window2' });
          const w2Shown = once(w2, 'show');
          w2.show();
          await w2Shown;

          expect(() => {
            w.moveAbove(w2.getMediaSourceId());
          }).to.not.throw();

          await closeWindow(w2, { assertNotWindows: false });
        });
      });

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
        it.skip('correctly returns whether a window is focusable', async () => {
          const w2 = new LynxWindow({ focusable: false });
          expect(w2.isFocusable()).to.be.false();

          w2.setFocusable(true);
          expect(w2.isFocusable()).to.be.true();
          await closeWindow(w2, { assertNotWindows: false });
        });
      });
    });
  });
});
