// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LynxWindow } from 'lynxtron';

import { expect } from 'chai';
import { once } from 'node:events';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as path from 'node:path';
import { setTimeout } from 'node:timers/promises';

import { listen } from './lib/spec-helpers';
import { closeAllWindows } from './lib/window-helpers';

describe('Lynx fetch', () => {
  let server: http.Server | undefined;

  afterEach(async () => {
    await closeAllWindows();
    if (server?.listening) {
      await new Promise<void>((resolve, reject) => {
        server!.close((error) => (error ? reject(error) : resolve()));
      });
    }
    server = undefined;
  });

  it('uses the Lynxtron HTTP service', async function () {
    this.timeout(30000);
    server = http.createServer((request, response) => {
      const chunks: Buffer[] = [];
      request.on('data', (chunk) => chunks.push(Buffer.from(chunk)));
      request.on('end', () => {
        expect(request.method).to.equal('POST');
        expect(request.headers['x-lynx-fetch']).to.equal('request-header');
        expect(Buffer.concat(chunks).toString()).to.equal('request-body');
        response.statusCode = 202;
        response.setHeader('x-lynx-fetch', 'response-header');
        response.end();
      });
    });
    const { url } = await listen(server);
    const window = new LynxWindow({ show: true });
    const messagePromise = once(window as any, '-lynx-message') as Promise<
      [string, any]
    >;
    const bundlePath = path.resolve(
      __dirname,
      './case/lynx-card/dist/lynx-fetch.lynx.bundle'
    );
    expect(fs.existsSync(bundlePath)).to.equal(true);
    const readyPromise = once(window, 'ready-to-show');
    expect(await window.loadFile(bundlePath)).to.equal(true);
    await readyPromise;

    await window.sendGlobalEvent('fetch_request', { url });
    const [method, result] = await Promise.race([
      messagePromise,
      setTimeout(10000).then(() => {
        throw new Error('Timed out waiting for Lynx fetch result');
      }),
    ]);

    expect(method).to.equal('fetch_result');
    expect(result).to.deep.equal({
      status: 202,
      responseHeader: 'response-header',
    });
  });
});
