// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { expect } from 'chai';
import * as fs from 'node:fs/promises';
import * as os from 'node:os';
import * as path from 'node:path';
import { pathToFileURL } from 'node:url';

import {
  onResourceFetcher,
  type LynxFetchReplayData,
} from '../lib/browser/api/lynx-resource-fetcher';

describe('Lynx resource fetcher', () => {
  let directory: string;
  let resourcesPathDescriptor: PropertyDescriptor | undefined;

  beforeEach(async () => {
    directory = await fs.mkdtemp(path.join(os.tmpdir(), 'lynx-assets-'));
    resourcesPathDescriptor = Object.getOwnPropertyDescriptor(
      process,
      'resourcesPath'
    );
    Object.defineProperty(process, 'resourcesPath', {
      configurable: true,
      value: directory,
    });
    await fs.writeFile(path.join(directory, 'lynx_core.js'), 'core-runtime');
  });

  afterEach(async () => {
    if (resourcesPathDescriptor) {
      Object.defineProperty(process, 'resourcesPath', resourcesPathDescriptor);
    } else {
      Reflect.deleteProperty(process, 'resourcesPath');
    }
    await fs.rm(directory, { recursive: true, force: true });
  });

  async function fetch(url: string): Promise<LynxFetchReplayData> {
    const replies: LynxFetchReplayData[] = [];
    await onResourceFetcher(
      { sendReply: (reply) => replies.push(reply) },
      'script',
      url
    );
    expect(replies).to.have.lengthOf(1);
    return replies[0];
  }

  it('loads the background runtime from assets://lynx_core.js', async () => {
    const reply = await fetch('assets://lynx_core.js');
    expect(reply.url).to.equal('assets://lynx_core.js');
    expect(reply.statusCode).to.equal(0);
    expect(reply.data.toString()).to.equal('core-runtime');
  });

  it('continues to load file URLs', async () => {
    const url = pathToFileURL(path.join(directory, 'lynx_core.js')).href;
    const reply = await fetch(url);
    expect(reply.statusCode).to.equal(0);
    expect(reply.data.toString()).to.equal('core-runtime');
  });

  for (const url of [
    'assets://../outside.js',
    'assets:///outside.js',
    'assets://missing.js',
  ]) {
    it(`returns a failure for ${url}`, async () => {
      const reply = await fetch(url);
      expect(reply.statusCode).to.equal(1);
      expect(reply.data.length).to.equal(0);
    });
  }

  it('returns a failure when resourcesPath is unavailable', async () => {
    Reflect.deleteProperty(process, 'resourcesPath');
    const reply = await fetch('assets://lynx_core.js');
    expect(reply.statusCode).to.equal(1);
    expect(reply.data.length).to.equal(0);
  });
});
