// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { app, LynxWindow } from 'lynxtron';
import * as cp from 'node:child_process';
import { once } from 'node:events';
import * as fs from 'node:fs';
import * as net from 'node:net';
import * as path from 'node:path';
import { setTimeout } from 'node:timers/promises';

import { assert, expect } from 'chai';
import split = require('split');

import { ifdescribe, ifit, waitUntil } from './lib/spec-helpers';
import { closeWindow, closeAllWindows } from './lib/window-helpers';

const fixturesPath = path.resolve(__dirname, 'fixtures');

const isMacOSx64 = process.platform === 'darwin' && process.arch === 'x64';

describe('lynxtron module', () => {
  describe('require("lynxtron")', () => {
    it('always returns the internal lynxtron module', () => {
      require('lynxtron');
    });
  });
});

describe('app module', () => {
  describe('app.getVersion()', () => {
    it('returns the version field of package.json', () => {
      expect(app.getVersion()).to.equal('0.1.0');
    });
  });

  ifdescribe(process.platform === 'darwin')('app events', () => {
    it('emits will-finish-launching before ready', async () => {
      const { fired, order } = await runTestApp('will-finish-launching');
      expect(fired).to.equal(true);
      expect(order[0]).to.equal('will-finish-launching');
      expect(order).to.include('ready');
    });
  });

  describe('app events', () => {
    it('emits before-quit', async () => {
      const { fired, order } = await runTestApp('before-quit');
      expect(fired).to.equal(true);
      expect(order[0]).to.equal('before-quit');
      expect(order).to.include('will-quit');
    });

    it('supports preventDefault in before-quit', async () => {
      const { fired, order, prevented } = await runTestApp(
        'before-quit',
        '--prevent-default'
      );
      expect(fired).to.equal(true);
      expect(prevented).to.equal(true);
      expect(order[0]).to.equal('before-quit');
      expect(order).to.not.include('will-quit');
    });

    it('supports preventDefault in will-quit', async () => {
      const { fired, order, prevented, willQuitFired } = await runTestApp(
        'before-quit',
        '--will-quit-test',
        '--prevent-will-quit'
      );
      expect(fired).to.equal(true);
      expect(willQuitFired).to.equal(true);
      expect(prevented).to.equal(true);
      expect(order[0]).to.equal('before-quit');
      expect(order).to.include('will-quit');
    });
  });

  describe('quit event', () => {
    it('includes exitCode', async () => {
      const { exitCode } = await runTestApp('quit-exit-code');
      expect(exitCode).to.equal(3);
    });
  });

  describe('app.setVersion(version)', () => {
    it('overrides the version', () => {
      expect(app.getVersion()).to.equal('0.1.0');
      app.setVersion('test-version');

      expect(app.getVersion()).to.equal('test-version');
      app.setVersion('0.1.0');
    });
  });

  describe('app name APIs', () => {
    describe('with properties', () => {
      it('returns the name field of package.json', () => {
        expect(app.name).to.equal('Electron Test Main');
      });

      it('overrides the name', () => {
        expect(app.name).to.equal('Electron Test Main');
        app.name = 'electron-test-name';

        expect(app.name).to.equal('electron-test-name');
        app.name = 'Electron Test Main';
      });
    });

    describe('with functions', () => {
      it('returns the name field of package.json', () => {
        expect(app.getName()).to.equal('Electron Test Main');
      });

      it('overrides the name', () => {
        expect(app.getName()).to.equal('Electron Test Main');
        app.setName('electron-test-name');

        expect(app.getName()).to.equal('electron-test-name');
        app.setName('Electron Test Main');
      });
    });
  });

  describe('app.getLocale()', () => {
    it.skip('should not be empty', () => {
      expect(app.getLocale()).to.not.equal('');
    });
  });

  describe('app.isPackaged', () => {
    it.skip('should be false during tests', () => {
      expect(app.isPackaged).to.equal(false);
    });
  });

  ifdescribe(process.platform === 'darwin')(
    'app.isInApplicationsFolder()',
    () => {
      it('should be false during tests', () => {
        expect(app.isInApplicationsFolder()).to.equal(false);
      });
    }
  );

  describe('app.exit(exitCode)', () => {
    let appProcess: cp.ChildProcess | null = null;

    afterEach(() => {
      if (appProcess) appProcess.kill();
    });

    it('emits a process exit event with the code', async () => {
      const appPath = path.join(fixturesPath, 'api', 'quit-app');
      const electronPath = process.execPath;
      let output = '';

      appProcess = cp.spawn(electronPath, [appPath]);
      if (appProcess && appProcess.stdout) {
        appProcess.stdout.on('data', (data) => {
          output += data;
        });
      }
      const [code] = await once(appProcess, 'exit');

      if (process.platform !== 'win32') {
        expect(output).to.include('Exit event with code: 123');
      }
      expect(code).to.equal(123);
    });

    it('closes all windows', async function () {
      const appPath = path.join(
        fixturesPath,
        'api',
        'exit-closes-all-windows-app'
      );
      const electronPath = process.execPath;

      appProcess = cp.spawn(electronPath, [appPath]);
      const [code, signal] = await once(appProcess, 'exit');

      expect(signal).to.equal(
        null,
        'exit signal should be null, if you see this please tag @MarshallOfSound'
      );
      expect(code).to.equal(
        123,
        'exit code should be 123, if you see this please tag @MarshallOfSound'
      );
    });

    ifit(['darwin', 'linux'].includes(process.platform))(
      'exits gracefully',
      async function () {
        const electronPath = process.execPath;
        const appPath = path.join(fixturesPath, 'api', 'singleton');
        appProcess = cp.spawn(electronPath, [appPath]);

        // Singleton will send us greeting data to let us know it's running.
        // After that, ask it to exit gracefully and confirm that it does.
        if (appProcess && appProcess.stdout) {
          appProcess.stdout.on('data', () => appProcess!.kill());
        }
        const [code, signal] = await once(appProcess, 'exit');

        const message = `code:\n${code}\nsignal:\n${signal}`;
        expect(code).to.equal(0, message);
        expect(signal).to.equal(null, message);
      }
    );
  });

  ifdescribe(process.platform === 'darwin')('app.setActivationPolicy', () => {
    it('throws an error on invalid application policies', () => {
      expect(() => {
        app.setActivationPolicy('terrible' as any);
      }).to.throw(
        /Invalid activation policy: must be one of 'regular', 'accessory', or 'prohibited'/
      );
    });
  });

  describe('app.requestSingleInstanceLock', () => {
    interface SingleInstanceLockTestArgs {
      args: string[];
      expectedAdditionalData: unknown;
    }

    it('prevents the second launch of app', async function () {
      this.timeout(120000);
      const appPath = path.join(fixturesPath, 'api', 'singleton-data');
      const first = cp.spawn(process.execPath, [appPath]);
      await once(first.stdout, 'data');
      // Start second app when received output.
      const second = cp.spawn(process.execPath, [appPath]);
      const [code2] = await once(second, 'exit');
      expect(code2).to.equal(1);
      const [code1] = await once(first, 'exit');
      expect(code1).to.equal(0);
    });

    it('returns true when setting non-existent user data folder', async function () {
      const appPath = path.join(fixturesPath, 'api', 'singleton-userdata');
      const instance = cp.spawn(process.execPath, [appPath]);
      const [code] = await once(instance, 'exit');
      expect(code).to.equal(0);
    });

    async function testArgumentPassing(testArgs: SingleInstanceLockTestArgs) {
      const appPath = path.join(fixturesPath, 'api', 'singleton-data');
      const first = cp.spawn(process.execPath, [appPath, ...testArgs.args]);
      const firstExited = once(first, 'exit');

      // Wait for the first app to boot.
      const firstStdoutLines = first.stdout.pipe(split());
      while ((await once(firstStdoutLines, 'data')).toString() !== 'started') {
        // wait.
      }
      const additionalDataPromise = once(firstStdoutLines, 'data');

      const secondInstanceArgs = [
        process.execPath,
        appPath,
        ...testArgs.args,
        '--some-switch',
        'some-arg',
      ];
      const second = cp.spawn(
        secondInstanceArgs[0],
        secondInstanceArgs.slice(1)
      );
      const secondExited = once(second, 'exit');

      const [code2] = await secondExited;
      expect(code2).to.equal(1);
      const [code1] = await firstExited;
      expect(code1).to.equal(0);
      const dataFromSecondInstance = await additionalDataPromise;
      const [args, additionalData] = dataFromSecondInstance[0]
        .toString('ascii')
        .split('||');
      const secondInstanceArgsReceived: string[] = JSON.parse(
        args.toString('ascii')
      );
      const secondInstanceDataReceived = JSON.parse(
        additionalData.toString('ascii')
      );

      // Ensure secondInstanceArgs is a subset of secondInstanceArgsReceived
      for (const arg of secondInstanceArgs) {
        expect(secondInstanceArgsReceived).to.include(
          arg,
          `argument ${arg} is missing from received second args`
        );
      }
      expect(secondInstanceDataReceived).to.be.deep.equal(
        testArgs.expectedAdditionalData,
        `received data ${JSON.stringify(
          secondInstanceDataReceived
        )} is not equal to expected data ${JSON.stringify(
          testArgs.expectedAdditionalData
        )}.`
      );
    }

    it('passes arguments to the second-instance event no additional data', async () => {
      await testArgumentPassing({
        args: [],
        expectedAdditionalData: null,
      });
    });

    it('sends and receives JSON object data', async () => {
      const expectedAdditionalData = {
        level: 1,
        testkey: 'testvalue1',
        inner: {
          level: 2,
          testkey: 'testvalue2',
        },
      };
      await testArgumentPassing({
        args: ['--send-data'],
        expectedAdditionalData,
      });
    });

    it('sends and receives numerical data', async () => {
      await testArgumentPassing({
        args: ['--send-data', '--data-content=2'],
        expectedAdditionalData: 2,
      });
    });

    it('sends and receives string data', async () => {
      await testArgumentPassing({
        args: ['--send-data', '--data-content="data"'],
        expectedAdditionalData: 'data',
      });
    });

    it('sends and receives boolean data', async () => {
      await testArgumentPassing({
        args: ['--send-data', '--data-content=false'],
        expectedAdditionalData: false,
      });
    });

    it('sends and receives array data', async () => {
      await testArgumentPassing({
        args: ['--send-data', '--data-content=[2, 3, 4]'],
        expectedAdditionalData: [2, 3, 4],
      });
    });

    it('sends and receives mixed array data', async () => {
      await testArgumentPassing({
        args: ['--send-data', '--data-content=["2", true, 4]'],
        expectedAdditionalData: ['2', true, 4],
      });
    });

    it('sends and receives null data', async () => {
      await testArgumentPassing({
        args: ['--send-data', '--data-content=null'],
        expectedAdditionalData: null,
      });
    });

    it('cannot send or receive undefined data', async () => {
      try {
        await testArgumentPassing({
          args: [
            '--send-ack',
            '--ack-content="undefined"',
            '--prevent-default',
            '--send-data',
            '--data-content="undefined"',
          ],
          expectedAdditionalData: undefined,
        });
        assert(false);
      } catch {
        // This is expected.
      }
    });
  });

  // GitHub Actions macOS-13 runners used for x64 seem to have a problem with this test.
  ifdescribe(process.platform !== 'linux' && !isMacOSx64)(
    'app.{add|get|clear}RecentDocument(s)',
    () => {
      const tempFiles = [
        path.join(fixturesPath, 'foo.txt'),
        path.join(fixturesPath, 'bar.txt'),
        path.join(fixturesPath, 'baz.txt'),
      ];

      afterEach(() => {
        app.clearRecentDocuments();
        for (const file of tempFiles) {
          fs.unlinkSync(file);
        }
      });

      beforeEach(() => {
        for (const file of tempFiles) {
          fs.writeFileSync(file, 'Lorem Ipsum');
        }
      });

      it('can add a recent document', async () => {
        app.addRecentDocument(tempFiles[0]);
        await setTimeout(2000);
        expect(app.getRecentDocuments()).to.include.members([tempFiles[0]]);
      });

      it('can clear recent documents', async () => {
        app.addRecentDocument(tempFiles[1]);
        app.addRecentDocument(tempFiles[2]);
        await setTimeout(2000);
        expect(app.getRecentDocuments()).to.include.members([
          tempFiles[1],
          tempFiles[2],
        ]);
        app.clearRecentDocuments();
        await setTimeout(2000);
        expect(app.getRecentDocuments()).to.deep.equal([]);
      });
    }
  );

  describe('app.relaunch', () => {
    let server: net.Server | null = null;
    // TODO(Guo Xi): change socketPath name
    const socketPath =
      process.platform === 'win32'
        ? '\\\\.\\pipe\\lynxtron-app-relaunch'
        : '/tmp/lynxtron-app-relaunch';

    beforeEach((done) => {
      fs.unlink(socketPath, () => {
        server = net.createServer();
        server.listen(socketPath);
        done();
      });
    });

    afterEach((done) => {
      server!.close(() => {
        if (process.platform === 'win32') {
          done();
        } else {
          fs.unlink(socketPath, () => done());
        }
      });
    });

    it('relaunches the app', function (done) {
      this.timeout(120000);

      let state = 'none';
      server!.once('error', (error) => done(error));
      server!.on('connection', (client) => {
        client.once('data', (data) => {
          if (String(data) === '--first' && state === 'none') {
            state = 'first-launch';
          } else if (String(data) === '--second' && state === 'first-launch') {
            state = 'second-launch';
          } else if (String(data) === '--third' && state === 'second-launch') {
            done();
          } else {
            done(`Unexpected state: "${state}", data: "${data}"`);
          }
        });
      });

      const appPath = path.join(fixturesPath, 'api', 'relaunch');
      const child = cp.spawn(process.execPath, [appPath, '--first']);
      child.stdout.on('data', (c) => console.log(c.toString()));
      child.stderr.on('data', (c) => console.log(c.toString()));
      child.on('exit', (code, signal) => {
        if (code !== 0) {
          console.log(`Process exited with code "${code}" signal "${signal}"`);
        }
      });
    });
  });

  ifdescribe(process.platform === 'darwin')(
    'app.setUserActivity(type, userInfo)',
    () => {
      it('sets the current activity', () => {
        app.setUserActivity('com.electron.testActivity', { testData: '123' });
        expect(app.getCurrentActivityType()).to.equal(
          'com.electron.testActivity'
        );
      });
    }
  );

  describe('LynxWindow events', () => {
    let w: LynxWindow = null as any;

    afterEach(() => {
      closeWindow(w).then(() => {
        w = null as any;
      });
    });

    it('should emit lynx-window-focus event when window is focused', async () => {
      const emitted = once(app, 'lynx-window-focus') as Promise<
        [any, LynxWindow]
      >;
      w = new LynxWindow({ show: false });
      w.emit('focus');
      const [, window] = await emitted;
      expect(window.id).to.equal(w.id);
    });

    it('should emit lynx-window-blur event when window is blurred', async () => {
      const emitted = once(app, 'lynx-window-blur') as Promise<
        [any, LynxWindow]
      >;
      w = new LynxWindow({ show: false });
      w.emit('blur');
      const [, window] = await emitted;
      expect(window.id).to.equal(w.id);
    });

    it('should emit lynx-window-created event when window is created', async () => {
      const emitted = once(app, 'lynx-window-created') as Promise<
        [any, LynxWindow]
      >;
      w = new LynxWindow({ show: false });
      const [, window] = await emitted;
      expect(window.id).to.equal(w.id);
    });
  });

  // TODO(Guo Xi): app.badgeCount
  // TODO(Guo Xi): app.get/setLoginItemSettings API
  // TODO(Guo Xi): accessibility support functionality
  // TODO(Guo Xi): setJumpList(categories)

  describe('getAppPath', () => {
    it('works for directories with package.json', async () => {
      const { appPath } = await runTestApp('app-path');
      expect(appPath).to.equal(path.resolve(fixturesPath, 'api/app-path'));
    });

    it('works for directories with index.js', async () => {
      const { appPath } = await runTestApp('app-path/lib');
      expect(appPath).to.equal(path.resolve(fixturesPath, 'api/app-path/lib'));
    });

    it('works for files without extension', async () => {
      const { appPath } = await runTestApp('app-path/lib/index');
      expect(appPath).to.equal(path.resolve(fixturesPath, 'api/app-path/lib'));
    });

    it('works for files', async () => {
      const { appPath } = await runTestApp('app-path/lib/index.js');
      expect(appPath).to.equal(path.resolve(fixturesPath, 'api/app-path/lib'));
    });
  });

  describe('getPath(name)', () => {
    it('returns paths that exist', () => {
      const paths = [
        fs.existsSync(app.getPath('exe')),
        fs.existsSync(app.getPath('home')),
        fs.existsSync(app.getPath('temp')),
      ];
      expect(paths).to.deep.equal([true, true, true]);
    });

    if (process.platform === 'darwin') {
      it('throws an error when trying to get the assets path on macOS', () => {
        expect(() => {
          app.getPath('assets' as any);
        }).to.throw(/Failed to get 'assets' path/);
      });
    } else {
      it('returns an assets path that is identical to the module path', () => {
        const assetsPath = app.getPath('assets');
        expect(fs.existsSync(assetsPath)).to.be.true();
        expect(assetsPath).to.equal(path.dirname(app.getPath('module')));
      });
    }

    it('throws an error when the name is invalid', () => {
      expect(() => {
        app.getPath('does-not-exist' as any);
      }).to.throw(/Failed to get 'does-not-exist' path/);
    });

    it('returns the overridden path', () => {
      app.setPath('music', __dirname);
      expect(app.getPath('music')).to.equal(__dirname);
    });

    if (process.platform === 'win32') {
      it('gets the folder for recent files', () => {
        const recent = app.getPath('recent');

        // We expect that one of our test machines have overridden this
        // to be something crazy, it'll always include the word "Recent"
        // unless people have been registry-hacking like crazy
        expect(recent).to.include('Recent');
      });

      it('can override the recent files path', () => {
        app.setPath('recent', 'C:\\fake-path');
        expect(app.getPath('recent')).to.equal('C:\\fake-path');
      });
    }

    it('uses the app name in getPath(userData)', () => {
      expect(app.getPath('userData')).to.include(app.name);
    });
  });

  // TODO(Guo Xi): setPath(name, path)
  describe('setAppLogsPath(path)', () => {
    it('throws when a relative path is passed', () => {
      const badPath = 'hey/hi/hello';

      expect(() => {
        app.setAppLogsPath(badPath);
      }).to.throw(/Path must be absolute/);
    });
  });

  // TODO(Guo Xi): setAsDefaultProtocolClient(protocol, path, args)
  // TODO(Guo Xi): getApplicationNameForProtocol
  // TODO(Guo Xi): getApplicationInfoForProtocol
  // TODO(Guo Xi): isDefaultProtocolClient()
  // TODO(Guo Xi): app launch through uri
  ifdescribe(process.platform !== 'linux')('getFileIcon() API', () => {
    const iconPath = path.join(__dirname, 'fixtures/assets/icon.ico');
    const sizes = {
      small: 16,
      normal: 32,
      large: 256,
    };

    it('fetches a non-empty icon', async () => {
      const icon = await app.getFileIcon(iconPath);
      expect(icon.isEmpty()).to.equal(false);
    });

    it('fetches normal icon size by default', async () => {
      const icon = await app.getFileIcon(iconPath);
      const size = icon.getSize();

      expect(size.height).to.equal(sizes.normal);
      expect(size.width).to.equal(sizes.normal);
    });

    describe('size option', () => {
      it('fetches a small icon', async () => {
        const icon = await app.getFileIcon(iconPath, { size: 'small' });
        const size = icon.getSize();

        expect(size.height).to.equal(sizes.small);
        expect(size.width).to.equal(sizes.small);
      });

      it('fetches a normal icon', async () => {
        const icon = await app.getFileIcon(iconPath, { size: 'normal' });
        const size = icon.getSize();

        expect(size.height).to.equal(sizes.normal);
        expect(size.width).to.equal(sizes.normal);
      });

      it('fetches a large icon', async () => {
        const icon = await app.getFileIcon(iconPath, { size: 'large' });
        const size = icon.getSize();

        expect(size.height).to.equal(sizes.large);
        expect(size.width).to.equal(sizes.large);
      });
    });
  });

  describe('getAppMetrics() API', () => {
    it('returns memory and cpu stats of the current process', () => {
      const appMetrics = app.getAppMetrics();
      expect(appMetrics).to.be.an('object');

      expect(appMetrics.creationTime).to.be.a('number').that.is.greaterThan(0);
      expect(appMetrics.cpu)
        .to.have.ownProperty('percentCPUUsage')
        .that.is.a('number');
      expect(appMetrics.cpu)
        .to.have.ownProperty('cumulativeCPUUsage')
        .that.is.a('number');
      expect(appMetrics.cpu)
        .to.have.ownProperty('idleWakeupsPerSecond')
        .that.is.a('number');
      expect(appMetrics.memory)
        .to.have.property('workingSetSize')
        .that.is.greaterThan(0);
      expect(appMetrics.memory)
        .to.have.property('peakWorkingSetSize')
        .that.is.greaterThan(0);
      if (process.platform === 'win32') {
        expect(appMetrics.memory)
          .to.have.property('privateBytes')
          .that.is.greaterThan(0);
      }
    });
  });

  // TODO(Guo Xi): getGPUFeatureStatus() API
  // TODO(Guo Xi): getGPUInfo() API
  // TODO(Guo Xi): sandbox options

  ifdescribe(process.platform === 'darwin')('app hide and show API', () => {
    describe('app.isHidden', () => {
      it('returns true when the app is hidden', async () => {
        app.hide();
        await expect(
          waitUntil(() => app.isHidden())
        ).to.eventually.be.fulfilled();
      });
      it('returns false when the app is shown', async () => {
        app.show();
        await expect(
          waitUntil(() => !app.isHidden())
        ).to.eventually.be.fulfilled();
      });
    });
  });

  // TODO(Guo Xi): support dock API on macOS
  // ifdescribe(process.platform === 'darwin')('dock APIs', () => {
  //   after(async () => {
  //     await app.dock?.show();
  //   });

  //   describe('dock.setMenu', () => {
  //     it('can be retrieved via dock.getMenu', () => {
  //       expect(app.dock?.getMenu()).to.equal(null);
  //       const menu = new Menu();
  //       app.dock?.setMenu(menu);
  //       expect(app.dock?.getMenu()).to.equal(menu);
  //     });

  //     it('keeps references to the menu', () => {
  //       app.dock?.setMenu(new Menu());
  //       const v8Util = process._linkedBinding('lynxtron_binding_v8_util');
  //       v8Util.requestGarbageCollectionForTesting();
  //     });
  //   });

  //   describe('dock.setIcon', () => {
  //     it('throws a descriptive error for a bad icon path', () => {
  //       const badPath = path.resolve('I', 'Do', 'Not', 'Exist');
  //       expect(() => {
  //         app.dock?.setIcon(badPath);
  //       }).to.throw(/Failed to load image from path (.+)/);
  //     });
  //   });

  //   describe('dock.bounce', () => {
  //     it('should return -1 for unknown bounce type', () => {
  //       expect(app.dock?.bounce('bad type' as any)).to.equal(-1);
  //     });

  //     it('should return a positive number for informational type', () => {
  //       const appHasFocus = !!LynxWindow.getFocusedWindow();
  //       if (!appHasFocus) {
  //         expect(app.dock?.bounce('informational')).to.be.at.least(0);
  //       }
  //     });

  //     it('should return a positive number for critical type', () => {
  //       const appHasFocus = !!LynxWindow.getFocusedWindow();
  //       if (!appHasFocus) {
  //         expect(app.dock?.bounce('critical')).to.be.at.least(0);
  //       }
  //     });
  //   });

  //   describe('dock.cancelBounce', () => {
  //     it('should not throw', () => {
  //       app.dock?.cancelBounce(app.dock?.bounce('critical'));
  //     });
  //   });

  //   describe('dock.setBadge', () => {
  //     after(() => {
  //       app.dock?.setBadge('');
  //     });

  //     it('should not throw', () => {
  //       app.dock?.setBadge('1');
  //     });

  //     it('should be retrievable via getBadge', () => {
  //       app.dock?.setBadge('test');
  //       expect(app.dock?.getBadge()).to.equal('test');
  //     });
  //   });

  //   describe('dock.hide', () => {
  //     it('should not throw', () => {
  //       app.dock?.hide();
  //       expect(app.dock?.isVisible()).to.equal(false);
  //     });
  //   });

  //   // Note that dock.show tests should run after dock.hide tests, to work
  //   // around a bug of macOS.
  //   // See https://github.com/electron/electron/pull/25269 for more.
  //   describe('dock.show', () => {
  //     it('should not throw', () => {
  //       return app.dock?.show().then(() => {
  //         expect(app.dock?.isVisible()).to.equal(true);
  //       });
  //     });

  //     it('returns a Promise', () => {
  //       expect(app.dock?.show()).to.be.a('promise');
  //     });

  //     it('eventually fulfills', async () => {
  //       await expect(app.dock?.show()).to.eventually.be.fulfilled.equal(undefined);
  //     });
  //   });
  // });

  describe('whenReady', () => {
    it('returns a Promise', () => {
      expect(app.whenReady()).to.be.a('promise');
    });

    it('becomes fulfilled if the app is already ready', async () => {
      expect(app.isReady()).to.equal(true);
      await expect(app.whenReady()).to.be.eventually.fulfilled.equal(undefined);
    });
  });

  describe('app.applicationMenu', () => {
    it('has the applicationMenu property', () => {
      expect(app).to.have.property('applicationMenu');
    });
  });

  describe('commandLine.hasSwitch', () => {
    it('returns true when present', () => {
      app.commandLine.appendSwitch('foobar1');
      expect(app.commandLine.hasSwitch('foobar1')).to.equal(true);
    });

    it('returns false when not present', () => {
      expect(app.commandLine.hasSwitch('foobar2')).to.equal(false);
    });
  });

  describe('commandLine.hasSwitch (existing argv)', () => {
    it('returns true when present', async () => {
      const { hasSwitch } = await runTestApp('command-line', '--foobar');
      expect(hasSwitch).to.equal(true);
    });

    it('returns false when not present', async () => {
      const { hasSwitch } = await runTestApp('command-line');
      expect(hasSwitch).to.equal(false);
    });
  });

  describe('commandLine.getSwitchValue', () => {
    it('returns the value when present', () => {
      app.commandLine.appendSwitch('foobar', 'æøåü');
      expect(app.commandLine.getSwitchValue('foobar')).to.equal('æøåü');
    });

    it('returns an empty string when present without value', () => {
      app.commandLine.appendSwitch('foobar1');
      expect(app.commandLine.getSwitchValue('foobar1')).to.equal('');
    });

    it('returns an empty string when not present', () => {
      expect(app.commandLine.getSwitchValue('foobar2')).to.equal('');
    });
  });

  describe('commandLine.getSwitchValue (existing argv)', () => {
    it('returns the value when present', async () => {
      const { getSwitchValue } = await runTestApp(
        'command-line',
        '--foobar=test'
      );
      expect(getSwitchValue).to.equal('test');
    });

    it('returns an empty string when present without value', async () => {
      const { getSwitchValue } = await runTestApp('command-line', '--foobar');
      expect(getSwitchValue).to.equal('');
    });

    it('returns an empty string when not present', async () => {
      const { getSwitchValue } = await runTestApp('command-line');
      expect(getSwitchValue).to.equal('');
    });
  });

  describe('commandLine.removeSwitch', () => {
    it('no-ops a non-existent switch', async () => {
      expect(app.commandLine.hasSwitch('foobar3')).to.equal(false);
      app.commandLine.removeSwitch('foobar3');
      expect(app.commandLine.hasSwitch('foobar3')).to.equal(false);
    });

    it('removes an existing switch', async () => {
      app.commandLine.appendSwitch('foobar3', 'test');
      expect(app.commandLine.hasSwitch('foobar3')).to.equal(true);
      app.commandLine.removeSwitch('foobar3');
      expect(app.commandLine.hasSwitch('foobar3')).to.equal(false);
    });
  });

  // TODO(Guo Xi): app.setSecureKeyboardEntryEnabled is not supported
  // TODO(Guo Xi): configureHostResolver is not supported

  describe('about panel', () => {
    it('app.setAboutPanelOptions() does not crash', async () => {
      const didRun = await runTestApp('about-panel', '--set-options');
      expect(didRun).to.equal(true);
    });

    it('app.showAboutPanel() does not crash & runs asynchronously', async () => {
      const didRun = await runTestApp('about-panel', '--show-panel');
      expect(didRun).to.equal(true);
    });
  });
});

// TODO(Guo Xi): app.setProxy(options) is not supported
describe('default behavior', () => {
  describe('application menu', () => {
    it('creates the default menu if the app does not set it', async () => {
      const result = await runTestApp('default-menu');
      expect(result).to.equal(false);
    });

    it('does not create the default menu if the app sets a custom menu', async () => {
      const result = await runTestApp('default-menu', '--custom-menu');
      expect(result).to.equal(true);
    });

    it('does not create the default menu if the app sets a null menu', async () => {
      const result = await runTestApp('default-menu', '--null-menu');
      expect(result).to.equal(true);
    });
  });

  describe('window-all-closed', () => {
    afterEach(closeAllWindows);

    it('quits when the app does not handle the event', async () => {
      const result = await runTestApp('window-all-closed');
      expect(result).to.equal(false);
    });

    it('does not quit when the app handles the event', async () => {
      const result = await runTestApp('window-all-closed', '--handle-event');
      expect(result).to.equal(true);
    });

    it('should omit closed windows from getAllWindows', async () => {
      const w = new LynxWindow({ show: false });
      const len = new Promise((resolve) => {
        app.on('window-all-closed', () => {
          resolve(LynxWindow.getAllWindows().length);
        });
      });
      w.close();
      expect(await len).to.equal(0);
    });
  });

  // TODO(Guo Xi): app.userAgentFallback is not supported
  // describe('user agent fallback', () => {
  //   let initialValue: string;

  //   before(() => {
  //     initialValue = app.userAgentFallback!;
  //   });

  //   it('should have a reasonable default', () => {
  //     expect(initialValue).to.include(`Electron/${process.versions.electron}`);
  //     expect(initialValue).to.include(`Chrome/${process.versions.chrome}`);
  //   });

  //   it('should be overridable', () => {
  //     app.userAgentFallback = 'test-agent/123';
  //     expect(app.userAgentFallback).to.equal('test-agent/123');
  //   });

  //   it('should be restorable', () => {
  //     app.userAgentFallback = 'test-agent/123';
  //     app.userAgentFallback = '';
  //     expect(app.userAgentFallback).to.equal(initialValue);
  //   });
  // });

  // TODO(Guo Xi): app.setLoginItemSettings is not supported
  // describe('login event', () => {
  //   afterEach(closeAllWindows);
  //   let server: http.Server;
  //   let serverUrl: string;

  //   before(async () => {
  //     server = http.createServer((request, response) => {
  //       if (request.headers.authorization) {
  //         return response.end('ok');
  //       }
  //       response
  //         .writeHead(401, { 'WWW-Authenticate': 'Basic realm="Foo"' })
  //         .end();
  //     });

  //     serverUrl = (await listen(server)).url;
  //   });

  //   after(() => {
  //     server.close();
  //   });

  describe('running under ARM64 translation', () => {
    it('does not throw an error', () => {
      if (process.platform === 'darwin' || process.platform === 'win32') {
        expect(app.runningUnderARM64Translation).not.to.be.undefined();
        expect(() => {
          return app.runningUnderARM64Translation;
        }).not.to.throw();
      } else {
        expect(app.runningUnderARM64Translation).to.be.undefined();
      }
    });
  });
});

async function runTestApp(name: string, ...args: any[]) {
  const appPath = path.join(fixturesPath, 'api', name);
  const electronPath = process.execPath;
  const appProcess = cp.spawn(electronPath, [appPath, ...args]);

  let output = '';
  appProcess.stdout.on('data', (data) => {
    output += data;
  });

  await once(appProcess.stdout, 'end');

  return JSON.parse(output);
}
