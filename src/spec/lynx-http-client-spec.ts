// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { expect } from 'chai';
import * as http from 'node:http';

import { requestHttpBuffer } from '../lib/browser/api/lynx-http-client';
import { listen } from './lib/spec-helpers';

describe('Lynx HTTP client', () => {
  let server: http.Server | undefined;

  afterEach(async () => {
    if (server?.listening) {
      await new Promise<void>((resolve, reject) => {
        server!.close((error) => (error ? reject(error) : resolve()));
      });
    }
    server = undefined;
  });

  it('forwards the method, headers, and body', async () => {
    server = http.createServer((request, response) => {
      const chunks: Buffer[] = [];
      request.on('data', (chunk) => chunks.push(Buffer.from(chunk)));
      request.on('end', () => {
        response.statusCode = 201;
        response.statusMessage = 'Created';
        response.setHeader('x-request-method', request.method || '');
        response.setHeader('x-request-header', request.headers['x-test'] || '');
        response.end(Buffer.concat(chunks));
      });
    });
    const { url } = await listen(server);

    const result = await requestHttpBuffer({
      url,
      method: 'POST',
      headers: { 'x-test': 'header-value' },
      body: Buffer.from('request-body'),
    });

    expect(result.statusCode).to.equal(201);
    expect(result.statusText).to.equal('Created');
    expect(result.headers['x-request-method']).to.equal('POST');
    expect(result.headers['x-request-header']).to.equal('header-value');
    expect(result.data.toString()).to.equal('request-body');
  });

  it('changes POST to GET for a 303 redirect', async () => {
    server = http.createServer((request, response) => {
      if (request.url === '/redirect') {
        response.statusCode = 303;
        response.setHeader('location', '/result');
        response.end();
        return;
      }
      response.end(
        `${request.method}:${request.headers['content-length'] || ''}`
      );
    });
    const { url } = await listen(server);

    const result = await requestHttpBuffer({
      url: `${url}/redirect`,
      method: 'POST',
      headers: { 'content-length': '4' },
      body: Buffer.from('body'),
    });

    expect(result.data.toString()).to.equal('GET:');
    expect(result.url).to.equal(`${url}/result`);
  });

  it('rejects malformed redirect locations', async () => {
    server = http.createServer((_request, response) => {
      response.statusCode = 302;
      response.setHeader('location', 'http://[');
      response.end();
    });
    const { url } = await listen(server);

    await expect(requestHttpBuffer({ url })).to.eventually.be.rejectedWith(
      'Invalid URL'
    );
  });

  it('rejects unsupported protocols', async () => {
    await expect(
      requestHttpBuffer({ url: 'file:///tmp/not-an-http-request' })
    ).to.eventually.be.rejectedWith('Unsupported protocol: file:');
  });
});
