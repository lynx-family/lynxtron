// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { app, BaseWindow, LynxWindow } from 'lynxtron';

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

    ifit(process.platform === 'darwin')('supports tabbingIdentifier', () => {
      const w = new LynxWindow({
        show: false,
        tabbingIdentifier: 'tab-group',
      });
      expect(w.tabbingIdentifier).to.equal('tab-group');
      w.destroy();
    });

    it('does not crash or throw when passed an invalid icon', async () => {
      expect(() => {
        const w = new LynxWindow({
          icon: undefined,
        } as any);
        w.destroy();
      }).not.to.throw();
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

  // TODO(Guo Xi): Does LynxWindow load and unload need event?
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

          const enterFullScreen = once(w, 'enter-full-screen');
          w.fullScreen = true;
          await enterFullScreen;
          expect(w.fullScreen).to.equal(true);

          const leaveFullScreen = once(w, 'leave-full-screen');
          w.fullScreen = false;
          await leaveFullScreen;
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

      ifdescribe(process.platform === 'win32')(
        'LynxWindow modal parent enable state',
        () => {
          afterEach(closeAllWindows);

          it('disables parent while modal child is visible', async () => {
            const parent = new LynxWindow({ show: false });
            parent.show();
            await once(parent, 'show');

            expect(parent.isEnabled()).to.equal(true);

            const child = new LynxWindow({
              show: false,
              parent,
              modal: true,
            });

            child.showInactive();
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

        ifit(process.platform === 'win32')(
          'frameless window keeps client size across activation changes',
          async () => {
            const wFrameless = new LynxWindow({
              show: true,
              frame: false,
              resizable: true,
              thickFrame: true,
              width: 400,
              height: 300,
            });
            const wOther = new LynxWindow({
              show: true,
              frame: true,
              resizable: false,
              width: 200,
              height: 200,
            });

            await setTimeout(50);
            wFrameless.focus();
            await setTimeout(100);

            const beforeContentSize = wFrameless.getContentSize();
            const beforeWindowSize = wFrameless.getSize();
            expect(beforeContentSize[0]).to.equal(beforeWindowSize[0]);
            expect(beforeContentSize[1]).to.equal(beforeWindowSize[1]);

            wOther.focus();
            await setTimeout(150);

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
