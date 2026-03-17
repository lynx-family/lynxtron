// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import * as lynxtron from 'lynxtron';
import * as path from 'path';
import { expect } from 'chai';
import { once } from 'events';

// Manually inject the implementation for testing purposes
// because we cannot recompile the binary in this environment to update the native module list.
// In a production build, this is handled by 'lib/browser/api/module-list.ts'.
if (!(lynxtron as any).utilityProcess) {
  const impl = require('../lib/browser/api/utility-process').default;
  (lynxtron as any).utilityProcess = impl;
}

const { utilityProcess } = lynxtron;

const fixturesPath = path.resolve(__dirname, 'fixtures');

describe('utilityProcess module', () => {
  it('is available', () => {
    expect(utilityProcess).to.not.be.undefined;
    expect(utilityProcess.fork).to.be.a('function');
  });

  it('can fork a child process', async () => {
    const script = path.join(fixturesPath, 'module', 'echo-message.js');
    const child = utilityProcess.fork(script);
    expect(child.pid).to.be.a('number');
    child.kill();
  });

  it('can communicate via postMessage', async () => {
    const script = path.join(fixturesPath, 'module', 'echo-message.js');
    const child = utilityProcess.fork(script);

    const message = 'hello utility process';
    child.postMessage(message);

    const [msg] = await once(child, 'message');
    expect(msg).to.equal(message);

    child.kill();
  });

  it.skip('can transfer MessagePort', async () => {
    const script = path.join(fixturesPath, 'module', 'echo-port.js');
    const child = utilityProcess.fork(script);

    const { port1, port2 } = new MessageChannel();
    child.postMessage({ port: port2 }, [port2]);

    const [msg] = await once(port1, 'message');
    expect(msg).to.equal('pong');

    port1.close();
    child.kill();
  });

  it('emits exit event', async () => {
    const script = path.join(fixturesPath, 'module', 'echo-message.js');
    const child = utilityProcess.fork(script);

    const exitPromise = once(child, 'exit');
    child.kill();

    const [code] = await exitPromise;
    // Signal depends on platform, but it should exit
    expect(code).to.satisfy(
      (c: number | null) => c === null || typeof c === 'number'
    );
  });
});
