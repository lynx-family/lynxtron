// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { expect } from 'chai';

import * as childProcess from 'node:child_process';
import { once } from 'node:events';
import * as fs from 'node:fs';
import * as path from 'node:path';
import { EventEmitter } from 'node:stream';
import * as util from 'node:util';

import {
  copyMacOSFixtureApp,
  getCodesignIdentity,
  shouldRunCodesignTests,
  signApp,
  spawn,
} from './lib/codesign-helpers';
import { withTempDirectory } from './lib/fs-helpers';
import { ifdescribe } from './lib/spec-helpers';

const mainFixturesPath = path.resolve(__dirname, 'fixtures');

describe('node feature', () => {
  const fixtures = path.join(__dirname, 'fixtures');

  describe('child_process', () => {
    describe('child_process.fork', () => {
      it('Works in browser process', async () => {
        const child = childProcess.fork(
          path.join(fixtures, 'module', 'ping.js')
        );
        const message = once(child, 'message');
        child.send('message');
        const [msg] = await message;
        expect(msg).to.equal('message');
      });

      // TODO(Guo Xi): this test process won't exit automatically
      // it('Has its module searth paths restricted', async () => {
      //   const child = childProcess.fork(path.join(fixtures, 'module', 'module-paths.js'));
      //   const [msg] = await once(child, 'message');
      //   expect(msg.length).to.equal(2);
      // });
    });

    describe('child_process.spawn', () => {
      it('supports spawning Lynxtron as a node process via LYNXTRON_RUN_AS_NODE', async () => {
        const child = childProcess.spawn(
          process.execPath,
          [path.join(fixtures, 'module', 'run-as-node.js')],
          {
            env: {
              ...process.env,
              LYNXTRON_RUN_AS_NODE: 'true',
            },
          }
        );

        if (!child.stdout) {
          throw new Error('stdout is not available');
        }

        let output = '';
        child.stdout.on('data', (data: Buffer) => {
          output += data;
        });

        try {
          await once(child.stdout, 'close');
          expect(JSON.parse(output)).to.deep.equal({
            stdoutType: 'pipe',
            processType: 'undefined',
            window: 'undefined',
          });
        } finally {
          child.kill();
        }
      });
    });
  });

  describe('contexts', () => {
    describe('setTimeout called under Chromium event loop in browser process', () => {
      it('Can be scheduled in time', (done) => {
        setTimeout(done, 0);
      });

      it('Can be promisified', (done) => {
        util.promisify(setTimeout)(0).then(done);
      });
    });

    describe('setInterval called under Chromium event loop in browser process', () => {
      it('can be scheduled in time', (done) => {
        let interval: any = null;
        let clearing = false;
        const clear = () => {
          if (interval === null || clearing) return;

          // interval might trigger while clearing (remote is slow sometimes)
          clearing = true;
          clearInterval(interval);
          clearing = false;
          interval = null;
          done();
        };
        interval = setInterval(clear, 10);
      });
    });

    const suspendListeners = (
      emitter: EventEmitter,
      eventName: string,
      callback: (...args: any[]) => void
    ) => {
      const listeners = emitter.listeners(eventName) as ((
        ...args: any[]
      ) => void)[];
      emitter.removeAllListeners(eventName);
      emitter.once(eventName, (...args) => {
        emitter.removeAllListeners(eventName);
        for (const listener of listeners) {
          emitter.on(eventName, listener);
        }

        callback(...args);
      });
    };
    describe('error thrown in main process node context', () => {
      it('gets emitted as a process uncaughtException event', async () => {
        fs.readFile(__filename, () => {
          throw new Error('hello');
        });
        const result = await new Promise((resolve) =>
          suspendListeners(process, 'uncaughtException', (error) => {
            resolve(error.message);
          })
        );
        expect(result).to.equal('hello');
      });
    });

    describe('promise rejection in main process node context', () => {
      it('gets emitted as a process unhandledRejection event', async () => {
        fs.readFile(__filename, () => {
          Promise.reject(new Error('hello'));
        });
        const result = await new Promise((resolve) =>
          suspendListeners(process, 'unhandledRejection', (error) => {
            resolve(error.message);
          })
        );
        expect(result).to.equal('hello');
      });

      it('does not log the warning more than once when the rejection is unhandled', async () => {
        const appPath = path.join(
          mainFixturesPath,
          'api',
          'unhandled-rejection.js'
        );
        const appProcess = childProcess.spawn(process.execPath, [appPath]);

        let output = '';
        const out = (data: string) => {
          output += data;
          if (/UnhandledPromiseRejectionWarning/.test(data)) {
            appProcess.kill();
          }
        };
        appProcess.stdout!.on('data', out);
        appProcess.stderr!.on('data', out);

        await once(appProcess, 'exit');
        expect(/UnhandledPromiseRejectionWarning/.test(output)).to.equal(true);
        const matches = output.match(/Error: oops/gm);
        expect(matches).to.have.lengthOf(1);
      });

      it('does not log the warning more than once when the rejection is handled', async () => {
        const appPath = path.join(
          mainFixturesPath,
          'api',
          'unhandled-rejection-handled.js'
        );
        const appProcess = childProcess.spawn(process.execPath, [appPath]);

        let output = '';
        const out = (data: string) => {
          output += data;
        };
        appProcess.stdout!.on('data', out);
        appProcess.stderr!.on('data', out);

        const [code] = await once(appProcess, 'exit');
        expect(code).to.equal(0);
        expect(/UnhandledPromiseRejectionWarning/.test(output)).to.equal(false);
        const matches = output.match(/Error: oops/gm);
        expect(matches).to.have.lengthOf(1);
      });
    });
  });

  describe('NODE_OPTIONS', () => {
    let child: childProcess.ChildProcessWithoutNullStreams;
    let exitPromise: Promise<any[]>;

    it('Fails for options disallowed by Node.js itself', (done) => {
      after(async () => {
        const [code, signal] = await exitPromise;
        expect(signal).to.equal(null);

        // Exit code 9 indicates cli flag parsing failure
        expect(code).to.equal(9);
        child.kill();
      });

      const env = { ...process.env, NODE_OPTIONS: '--v8-options' };
      child = childProcess.spawn(process.execPath, { env });
      exitPromise = once(child, 'exit');

      let output = '';
      let success = false;
      const cleanup = () => {
        child.stderr.removeListener('data', listener);
        child.stdout.removeListener('data', listener);
      };

      const listener = (data: Buffer) => {
        output += data;
        if (
          /electron: --v8-options is not allowed in NODE_OPTIONS/m.test(output)
        ) {
          success = true;
          cleanup();
          done();
        }
      };

      child.stderr.on('data', listener);
      child.stdout.on('data', listener);
      child.on('exit', () => {
        if (!success) {
          cleanup();
          done(new Error(`Unexpected output: ${output.toString()}`));
        }
      });
    });

    it('Disallows crypto-related options', (done) => {
      after(() => {
        child.kill();
      });

      const appPath = path.join(fixtures, 'module', 'noop.js');
      const env = { ...process.env, NODE_OPTIONS: '--use-openssl-ca' };
      child = childProcess.spawn(
        process.execPath,
        ['--enable-logging', appPath],
        { env }
      );

      let output = '';
      const cleanup = () => {
        child.stderr.removeListener('data', listener);
        child.stdout.removeListener('data', listener);
      };

      const listener = (data: Buffer) => {
        output += data;
        if (
          /The NODE_OPTION --use-openssl-ca is not supported in Electron/m.test(
            output
          )
        ) {
          cleanup();
          done();
        }
      };

      child.stderr.on('data', listener);
      child.stdout.on('data', listener);
    });

    it('does allow --require in non-packaged apps', async () => {
      const appPath = path.join(fixtures, 'module', 'noop.js');
      const env = {
        ...process.env,
        NODE_OPTIONS: `--require=${path.join(fixtures, 'module', 'fail.js')}`,
      };
      // App should exit with code 1.
      const child = childProcess.spawn(process.execPath, [appPath], { env });
      const [code] = await once(child, 'exit');
      expect(code).to.equal(1);
    });

    it('does allow --require in utility process of non-packaged apps', async () => {
      const appPath = path.join(
        fixtures,
        'apps',
        'node-options-utility-process'
      );
      // App should exit with code 1.
      const child = childProcess.spawn(process.execPath, [appPath]);
      const [code] = await once(child, 'exit');
      expect(code).to.equal(1);
    });

    it('does not allow --require in packaged apps', async () => {
      const appPath = path.join(fixtures, 'module', 'noop.js');
      const env = {
        ...process.env,
        ELECTRON_FORCE_IS_PACKAGED: 'true',
        NODE_OPTIONS: `--require=${path.join(fixtures, 'module', 'fail.js')}`,
      };
      // App should exit with code 0.
      const child = childProcess.spawn(process.execPath, [appPath], { env });
      const [code] = await once(child, 'exit');
      expect(code).to.equal(0);
    });
  });

  ifdescribe(shouldRunCodesignTests)('NODE_OPTIONS in signed app', function () {
    let identity = '';

    beforeEach(function () {
      const result = getCodesignIdentity();
      if (result === null) {
        this.skip();
      } else {
        identity = result;
      }
    });

    const script = path.join(fixtures, 'api', 'fork-with-node-options.js');
    const nodeOptionsWarning =
      'Node.js environment variables are disabled because this process is invoked by other apps';

    it('is disabled when invoked by other apps in LYNXTRON_RUN_AS_NODE mode', async () => {
      await withTempDirectory(async (dir) => {
        const appPath = await copyMacOSFixtureApp(dir);
        await signApp(appPath, identity);
        // Invoke Electron by using the system node binary as middle layer, so
        // the check of NODE_OPTIONS will think the process is started by other
        // apps.
        const { code, out } = await spawn('node', [
          script,
          path.join(appPath, 'Contents/MacOS/Electron'),
        ]);
        expect(code).to.equal(0);
        expect(out).to.include(nodeOptionsWarning);
      });
    });

    it('is disabled when invoked by alien binary in app bundle in LYNXTRON_RUN_AS_NODE mode', async function () {
      await withTempDirectory(async (dir) => {
        const appPath = await copyMacOSFixtureApp(dir);
        await signApp(appPath, identity);
        // Find system node and copy it to app bundle.
        const nodePath = process.env.PATH?.split(path.delimiter).find((dir) =>
          fs.existsSync(path.join(dir, 'node'))
        );
        if (!nodePath) {
          this.skip();
          return;
        }
        const alienBinary = path.join(appPath, 'Contents/MacOS/node');
        await fs.promises.cp(path.join(nodePath, 'node'), alienBinary, {
          recursive: true,
        });
        // Try to execute electron app from the alien node in app bundle.
        const { code, out } = await spawn(alienBinary, [
          script,
          path.join(appPath, 'Contents/MacOS/Electron'),
        ]);
        expect(code).to.equal(0);
        expect(out).to.include(nodeOptionsWarning);
      });
    });

    it('is respected when invoked from self', async () => {
      await withTempDirectory(async (dir) => {
        const appPath = await copyMacOSFixtureApp(dir, null);
        await signApp(appPath, identity);
        const appExePath = path.join(appPath, 'Contents/MacOS/Electron');
        const { code, out } = await spawn(appExePath, [script, appExePath]);
        expect(code).to.equal(1);
        expect(out).to.not.include(nodeOptionsWarning);
        expect(out).to.include('NODE_OPTIONS passed to child');
      });
    });
  });

  describe('Node.js cli flags', () => {
    let child: childProcess.ChildProcessWithoutNullStreams;
    let exitPromise: Promise<any[]>;

    it('Prohibits crypto-related flags in LYNXTRON_RUN_AS_NODE mode', (done) => {
      after(async () => {
        const [code, signal] = await exitPromise;
        expect(signal).to.equal(null);
        expect(code).to.equal(9);
        child.kill();
      });

      child = childProcess.spawn(process.execPath, ['--force-fips'], {
        env: { LYNXTRON_RUN_AS_NODE: 'true' },
      });
      exitPromise = once(child, 'exit');

      let output = '';
      const cleanup = () => {
        child.stderr.removeListener('data', listener);
        child.stdout.removeListener('data', listener);
      };

      const listener = (data: Buffer) => {
        output += data;
        if (
          /.*The Node.js cli flag --force-fips is not supported in Electron/m.test(
            output
          )
        ) {
          cleanup();
          done();
        }
      };

      child.stderr.on('data', listener);
      child.stdout.on('data', listener);
    });
  });

  describe('process.stdout', () => {
    it('is a real Node stream', () => {
      expect((process.stdout as any)._type).to.not.be.undefined();
    });
  });

  describe('fs.readFile', () => {
    it('can accept a FileHandle as the Path argument', async () => {
      const filePathForHandle = path.resolve(
        mainFixturesPath,
        'dogs-running.txt'
      );
      const fileHandle = await fs.promises.open(filePathForHandle, 'r');

      const file = await fs.promises.readFile(fileHandle, { encoding: 'utf8' });
      expect(file).to.not.be.empty();
      await fileHandle.close();
    });
  });

  describe('inspector', () => {
    let child: childProcess.ChildProcessWithoutNullStreams;
    let exitPromise: Promise<any[]> | null;

    afterEach(async () => {
      if (child && exitPromise) {
        const [code, signal] = await exitPromise;
        expect(signal).to.equal(null);
        expect(code).to.equal(0);
      } else if (child) {
        child.kill();
      }
      child = null as any;
      exitPromise = null as any;
    });

    it('Supports starting the v8 inspector with --inspect/--inspect-brk', (done) => {
      child = childProcess.spawn(
        process.execPath,
        ['--inspect-brk', path.join(fixtures, 'module', 'run-as-node.js')],
        {
          env: { LYNXTRON_RUN_AS_NODE: 'true' },
        }
      );

      let output = '';
      const cleanup = () => {
        child.stderr.removeListener('data', listener);
        child.stdout.removeListener('data', listener);
      };

      const listener = (data: Buffer) => {
        output += data;
        if (/Debugger listening on ws:/m.test(output)) {
          cleanup();
          done();
        }
      };

      child.stderr.on('data', listener);
      child.stdout.on('data', listener);
    });

    it('Supports starting the v8 inspector with --inspect and a provided port', async () => {
      child = childProcess.spawn(
        process.execPath,
        ['--inspect=17364', path.join(fixtures, 'module', 'run-as-node.js')],
        {
          env: { LYNXTRON_RUN_AS_NODE: 'true' },
        }
      );
      exitPromise = once(child, 'exit');

      let output = '';
      const listener = (data: Buffer) => {
        output += data;
      };
      const cleanup = () => {
        child.stderr.removeListener('data', listener);
        child.stdout.removeListener('data', listener);
      };

      child.stderr.on('data', listener);
      child.stdout.on('data', listener);
      await once(child, 'exit');
      cleanup();
      if (/^Debugger listening on ws:/m.test(output)) {
        expect(output.trim()).to.contain(
          ':17364',
          'should be listening on port 17364'
        );
      } else {
        throw new Error(`Unexpected output: ${output.toString()}`);
      }
    });

    it('Does not start the v8 inspector when --inspect is after a -- argument', async () => {
      child = childProcess.spawn(process.execPath, [
        path.join(fixtures, 'module', 'noop.js'),
        '--',
        '--inspect',
      ]);
      exitPromise = once(child, 'exit');

      let output = '';
      const listener = (data: Buffer) => {
        output += data;
      };
      child.stderr.on('data', listener);
      child.stdout.on('data', listener);
      await once(child, 'exit');
      if (output.trim().startsWith('Debugger listening on ws://')) {
        throw new Error('Inspector was started when it should not have been');
      }
    });

    it.skip('Supports js binding', async () => {
      child = childProcess.spawn(
        process.execPath,
        ['--inspect', path.join(fixtures, 'module', 'inspector-binding.js')],
        {
          env: { LYNXTRON_RUN_AS_NODE: 'true' },
          stdio: ['ipc'],
        }
      ) as childProcess.ChildProcessWithoutNullStreams;
      exitPromise = once(child, 'exit');

      const [{ cmd, debuggerEnabled, success }] = await once(child, 'message');
      expect(cmd).to.equal('assert');
      expect(debuggerEnabled).to.be.true();
      expect(success).to.be.true();
    });
  });

  it('Can find a module using a package.json main field', () => {
    const result = childProcess.spawnSync(
      process.execPath,
      [path.resolve(fixtures, 'api', 'electron-main-module', 'app.asar')],
      { stdio: 'inherit' }
    );
    expect(result.status).to.equal(0);
  });

  it('handles Promise timeouts correctly', async () => {
    const scriptPath = path.join(fixtures, 'module', 'node-promise-timer.js');
    const child = childProcess.spawn(process.execPath, [scriptPath], {
      env: { LYNXTRON_RUN_AS_NODE: 'true' },
    });
    const [code, signal] = await once(child, 'exit');
    expect(code).to.equal(0);
    expect(signal).to.equal(null);
    child.kill();
  });

  it('performs microtask checkpoint correctly', (done) => {
    let timer: NodeJS.Timeout;
    const listener = () => {
      done(new Error('catch block is delayed to next tick'));
    };

    const f3 = async () => {
      return new Promise((resolve, reject) => {
        timer = setTimeout(listener);
        reject(new Error('oops'));
      });
    };

    setTimeout(() => {
      f3().catch(() => {
        clearTimeout(timer);
        done();
      });
    });
  });
});
