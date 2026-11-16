// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { LynxWindow } from 'lynxtron';
import type { Protocol } from '../packages/lynxtron/apis/api/protocol';

import { expect } from 'chai';
import { execFile } from 'node:child_process';
import * as fs from 'original-fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { pathToFileURL } from 'node:url';
import { promisify } from 'node:util';

import { closeAllWindows } from './lib/window-helpers';
import { loadAndWait } from './lib/lynx-window-helpers';

const { protocol } = require('lynxtron') as { protocol: Protocol };

const bundlePath = path.resolve(
  __dirname,
  './case/lynx-card/dist/bridging-lynx-node.lynx.bundle'
);
const execFileAsync = promisify(execFile);
const invalidTemplate = Buffer.from('This is not a Lynx bundle');

const loadAsarTemplate = async (
  tempDir: string,
  templateBytes: Buffer,
  interceptAssets = false
) => {
  const archivePath = path.join(tempDir, 'app.asar');
  const entry = { size: templateBytes.length, offset: '0' };
  const header = Buffer.from(
    JSON.stringify({ files: { 'main.lynx.bundle': entry } })
  );
  // ASAR stores a size pickle, a JSON-header pickle, then the file bytes.
  const headerSize = 8 + Math.ceil(header.length / 4) * 4;
  const archiveHeader = Buffer.alloc(8 + headerSize);
  archiveHeader.writeUInt32LE(4, 0);
  archiveHeader.writeUInt32LE(headerSize, 4);
  archiveHeader.writeUInt32LE(headerSize - 4, 8);
  archiveHeader.writeUInt32LE(header.length, 12);
  header.copy(archiveHeader, 16);
  fs.writeFileSync(archivePath, Buffer.concat([archiveHeader, templateBytes]));

  // Native ASAR handles remain cached until process exit, including on Windows.
  await execFileAsync(
    process.execPath,
    [
      path.join(__dirname, 'fixtures', 'module', 'asar-protocol.js'),
      path.join(archivePath, 'main.lynx.bundle'),
      bundlePath,
      String(interceptAssets),
    ],
    { timeout: 10000 }
  );
};

const loadFile = async (filePath: string, data?: Buffer) => {
  const url = pathToFileURL(filePath).href;
  let intercepted = false;
  protocol.handle('file', (request) => {
    if (request.url !== url) {
      return false;
    }
    intercepted = true;
    return data ? { url, statusCode: 0, data } : false;
  });
  expect(protocol.isProtocolHandled('file')).to.equal(true);

  const window = new LynxWindow({ width: 800, height: 600 });
  await loadAndWait(window, () => {
    expect(window.loadFile(filePath)).to.equal(true);
  });
  expect(intercepted).to.equal(true);
  expect(window.isDestroyed()).to.equal(false);
};

describe('protocol.handle', () => {
  let tempDir: string;
  let templateBytes: Buffer;

  before(function () {
    if (!fs.existsSync(bundlePath)) {
      this.skip();
    }
    templateBytes = fs.readFileSync(bundlePath);
  });

  beforeEach(() => {
    tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'lynxtron-protocol-'));
  });

  afterEach(async () => {
    protocol.unhandle('file');
    await closeAllWindows();
    fs.rmSync(tempDir, { recursive: true, force: true });
  });

  it('intercepts the main template loaded through LynxWindow.loadFile', async function () {
    const interceptedPath = path.join(tempDir, 'intercepted.lynx.bundle');
    fs.writeFileSync(interceptedPath, invalidTemplate);
    await loadFile(interceptedPath, templateBytes);
  });

  it('falls back to the built-in file loader when the handler returns false', async function () {
    await loadFile(bundlePath);
  });

  it('loads relative paths with percent-escaped file names', async function () {
    const copiedPath = path.join(tempDir, 'bundle #1 %23.lynx.bundle');
    fs.writeFileSync(copiedPath, templateBytes);
    await loadFile(path.relative(process.cwd(), copiedPath));
  });

  it('falls back to reading templates inside ASAR archives', async function () {
    this.timeout(15000);
    await loadAsarTemplate(tempDir, templateBytes);
  });

  it('rewrites and intercepts the Assets fallback after a Template request fails', async function () {
    this.timeout(15000);
    await loadAsarTemplate(tempDir, invalidTemplate, true);
  });

  it('rejects when the protocol handler returns an invalid template', async function () {
    await expect(loadFile(bundlePath, invalidTemplate)).to.be.rejectedWith(
      'Lynx load failed'
    );
  });
});
