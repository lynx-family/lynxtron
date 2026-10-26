// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { expect } from 'chai';
import * as childProcess from 'node:child_process';
import * as path from 'node:path';
import { promisify } from 'node:util';

import { app } from 'lynxtron';

import { ifdescribe, ifit } from './lib/spec-helpers';

interface StartupResult {
  ready: boolean;
  type: string;
  arch: string;
  platform: string;
  totalMemory: number;
  packageName: string;
  digest: string;
  bufferHex: string;
}

interface NodeModeResult {
  type: string;
  arch: string;
  platform: string;
  nodeVersion: string;
  lynxtronVersion: string;
  totalMemory: number;
  fixtureLoaded: boolean;
  digest: string;
}

const fixturePath = path.join(__dirname, 'fixtures', 'node-snapshot-startup');
const resultPrefix = 'NODE_SNAPSHOT_RESULT ';
const nodeModeResultPrefix = 'NODE_MODE_SNAPSHOT_RESULT ';
const execFileAsync = promisify(childProcess.execFile);
const supportsNodeStartupSnapshot =
  (process.platform === 'darwin' &&
    (process.arch === 'arm64' || process.arch === 'x64')) ||
  (process.platform === 'win32' &&
    (process.arch === 'x64' || process.arch === 'ia32')) ||
  (process.platform === 'linux' &&
    (process.arch === 'x64' || process.arch === 'arm64'));
// Cross-architecture builds can use the snapshot without Node's code cache.
const hasNodeCodeCache =
  (process.config.variables as Record<string, unknown>)
    .node_use_node_code_cache === true;

async function runFixture<T>(
  entry: string,
  prefix: string,
  runAsNode: boolean
): Promise<{
  result: T;
  stderr: string;
}> {
  const env: NodeJS.ProcessEnv = {
    ...process.env,
    NODE_DEBUG_NATIVE: runAsNode ? 'MKSNAPSHOT,CODE_CACHE' : 'CODE_CACHE',
  };
  if (runAsNode) {
    env.LYNXTRON_RUN_AS_NODE = 'true';
  } else {
    delete env.LYNXTRON_RUN_AS_NODE;
  }

  const { stdout, stderr } = await execFileAsync(process.execPath, [entry], {
    env,
    timeout: 30000,
    maxBuffer: 1024 * 1024,
  });
  const resultLine = stdout
    .split(/\r?\n/)
    .find((line) => line.startsWith(prefix));
  expect(resultLine, `Fixture did not report a result:\n${stdout}`).to.be.a(
    'string'
  );
  return {
    result: JSON.parse(resultLine!.slice(prefix.length)) as T,
    stderr,
  };
}

function expectNodeCodeCache(stderr: string): void {
  const accepted = [
    ...stderr.matchAll(/^Code cache of (\S+) \([^)]+\) is accepted\r?$/gm),
  ].map((match) => match[1]);
  const rejected = [
    ...stderr.matchAll(/^Code cache of (\S+) \([^)]+\) is rejected\r?$/gm),
  ].map((match) => match[1]);

  expect(stderr).to.match(/^snapshot contains [1-9]\d* code cache\r?$/m);
  expect(
    rejected,
    'Node builtin code caches rejected at startup'
  ).to.deep.equal([]);
  expect(accepted).to.include('os');
  expect(accepted).to.include('crypto');
}

ifdescribe(supportsNodeStartupSnapshot)('Node startup snapshot', () => {
  it('exposes the browser Node environment after app ready', () => {
    expect(app.isReady()).to.equal(true);
    expect(
      (process.config.variables as Record<string, unknown>)
        .node_use_node_snapshot
    ).to.equal(true);
    expect((process as NodeJS.Process & { type?: string }).type).to.equal(
      'browser'
    );
  });

  describe('clean process startup', () => {
    let fixtureRun: { result: StartupResult; stderr: string };
    before(async function () {
      this.timeout(45000);
      fixtureRun = await runFixture<StartupResult>(
        fixturePath,
        resultPrefix,
        false
      );
    });

    it('loads Node builtins after app ready', () => {
      const { result } = fixtureRun;
      expect(result.ready).to.equal(true);
      expect(result.type).to.equal('browser');
      expect(result.arch).to.equal(process.arch);
      expect(result.platform).to.equal(process.platform);
      expect(result.totalMemory).to.be.greaterThan(0);
      expect(result.packageName).to.equal(
        'lynxtron-node-snapshot-startup-fixture'
      );
      expect(result.digest).to.equal(
        'f6b29201dcf6358e3dd02ff4b41792fe1c6326f92d408c29ceacf54e6bfc3b7b'
      );
      expect(result.bufferHex).to.equal('6e6f6465');
    });

    ifit(hasNodeCodeCache)('consumes Node builtin code cache', () => {
      expectNodeCodeCache(fixtureRun.stderr);
    });
  });

  describe('LYNXTRON_RUN_AS_NODE startup', () => {
    let fixtureRun: { result: NodeModeResult; stderr: string };
    before(async function () {
      this.timeout(45000);
      fixtureRun = await runFixture<NodeModeResult>(
        path.join(fixturePath, 'node-mode.js'),
        nodeModeResultPrefix,
        true
      );
    });

    it('restores the Node environment and runs a CLI script', () => {
      const { result, stderr } = fixtureRun;
      expect(result.type).to.equal('undefined');
      expect(result.arch).to.equal(process.arch);
      expect(result.platform).to.equal(process.platform);
      expect(result.nodeVersion).to.equal(process.versions.node);
      expect(result.lynxtronVersion).to.equal(process.versions.lynxtron);
      expect(result.totalMemory).to.be.greaterThan(0);
      expect(result.fixtureLoaded).to.equal(true);
      expect(result.digest).to.equal(
        'f6b29201dcf6358e3dd02ff4b41792fe1c6326f92d408c29ceacf54e6bfc3b7b'
      );
      // These diagnostics confirm both snapshot metadata and Environment
      // deserialization, rather than only a successful Node CLI startup.
      expect(stderr).to.match(/^snapshot contains \d+ code cache\r?$/m);
      expect(stderr).to.match(/^deserializing EnvSerializeInfo\.\.\.\r?$/m);
    });

    ifit(hasNodeCodeCache)('consumes Node builtin code cache', () => {
      expectNodeCodeCache(fixtureRun.stderr);
    });
  });
});
