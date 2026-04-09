import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

import { flipFuses, FuseV1Options, FuseVersion, getCurrentFuses } from './fuses.js';

const FUSE_SENTINEL = Buffer.from('dL7pKGdnNz796PbbjQWNKmHXBZaB9tsX', 'ascii');
const DEFAULT_WIRE = Buffer.from('11100', 'ascii');

function createSyntheticBinary(sliceCount = 1) {
  const slices = [];
  for (let index = 0; index < sliceCount; index += 1) {
    slices.push(
      Buffer.concat([
        Buffer.from(`prefix-${index}`),
        FUSE_SENTINEL,
        Buffer.from([FuseVersion.V1, DEFAULT_WIRE.length]),
        DEFAULT_WIRE,
        Buffer.from(`suffix-${index}`),
      ])
    );
  }

  return Buffer.concat(slices);
}

async function withTempBinary(sliceCount, callback) {
  const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-fuses-'));
  const binaryPath = path.join(tempDir, 'Lynxtron');

  try {
    await fs.writeFile(binaryPath, createSyntheticBinary(sliceCount));
    await callback(binaryPath);
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
}

async function withTempWindowsApp(sliceCount, callback) {
  const tempDir = await fs.mkdtemp(
    path.join(os.tmpdir(), 'lynxtron-fuses-win-app-')
  );
  const appPath = path.join(tempDir, 'lynxtron');
  const dllPath = path.join(appPath, 'lynxtron.dll');
  const exePath = path.join(appPath, 'lynxtron.exe');

  try {
    await fs.mkdir(appPath, { recursive: true });
    await fs.writeFile(dllPath, createSyntheticBinary(sliceCount));
    await fs.writeFile(exePath, Buffer.from('plain-exe-without-fuse-wire'));
    await callback({ appPath, dllPath, exePath });
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
}

async function withTempMacApp(sliceCount, callback) {
  const tempDir = await fs.mkdtemp(
    path.join(os.tmpdir(), 'lynxtron-fuses-mac-app-')
  );
  const appPath = path.join(tempDir, 'Lynxtron.app');
  const frameworkPath = path.join(
    appPath,
    'Contents',
    'Frameworks',
    'Lynxtron Framework.framework'
  );
  const frameworkBinaryPath = path.join(frameworkPath, 'Lynxtron Framework');
  const appBinaryPath = path.join(appPath, 'Contents', 'MacOS', 'Lynxtron');

  try {
    await fs.mkdir(path.dirname(frameworkBinaryPath), { recursive: true });
    await fs.mkdir(path.dirname(appBinaryPath), { recursive: true });
    await fs.writeFile(frameworkBinaryPath, createSyntheticBinary(sliceCount));
    await fs.writeFile(appBinaryPath, Buffer.from('plain-mac-launcher-without-fuse-wire'));
    await callback({ appPath, frameworkBinaryPath, appBinaryPath });
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
}

test('getCurrentFuses reads the current fuse wire', async () => {
  await withTempBinary(1, async (binaryPath) => {
    const fuses = await getCurrentFuses({ binary: binaryPath });

    assert.equal(fuses.binaryPath, binaryPath);
    assert.equal(fuses.version, FuseVersion.V1);
    assert.equal(fuses.runAsNode, true);
    assert.equal(fuses.nodeOptions, true);
    assert.equal(fuses.nodeCliInspect, true);
    assert.equal(fuses.embeddedAsarIntegrityValidation, false);
    assert.equal(fuses.onlyLoadAppFromAsar, false);
  });
});

test('flipFuses updates a single-slice binary', async () => {
  await withTempBinary(1, async (binaryPath) => {
    await flipFuses(
      { binary: binaryPath },
      {
        version: FuseVersion.V1,
        [FuseV1Options.RunAsNode]: false,
        [FuseV1Options.OnlyLoadAppFromAsar]: true,
      }
    );

    const fuses = await getCurrentFuses({ binary: binaryPath });
    assert.equal(fuses.runAsNode, false);
    assert.equal(fuses.nodeOptions, true);
    assert.equal(fuses.nodeCliInspect, true);
    assert.equal(fuses.embeddedAsarIntegrityValidation, false);
    assert.equal(fuses.onlyLoadAppFromAsar, true);
  });
});

test('flipFuses updates both slices of a universal-style binary', async () => {
  await withTempBinary(2, async (binaryPath) => {
    await flipFuses(
      { binary: binaryPath },
      {
        version: FuseVersion.V1,
        [FuseV1Options.EnableNodeOptionsEnvironmentVariable]: false,
        [FuseV1Options.EnableNodeCliInspectArguments]: false,
      }
    );

    const binary = await fs.readFile(binaryPath);
    const expectedWire = Buffer.from('10000', 'ascii');
    const firstOffset =
      binary.indexOf(FUSE_SENTINEL) + FUSE_SENTINEL.length + 2;
    const secondOffset =
      binary.indexOf(FUSE_SENTINEL, firstOffset) + FUSE_SENTINEL.length + 2;

    assert.ok(firstOffset >= FUSE_SENTINEL.length + 2);
    assert.ok(secondOffset >= FUSE_SENTINEL.length + 2);
    assert.equal(
      binary.subarray(firstOffset, firstOffset + expectedWire.length).toString(),
      expectedWire.toString()
    );
    assert.equal(
      binary.subarray(secondOffset, secondOffset + expectedWire.length).toString(),
      expectedWire.toString()
    );
  });
});

test('getCurrentFuses resolves a Windows app directory to lynxtron.dll', async () => {
  await withTempWindowsApp(1, async ({ appPath, dllPath, exePath }) => {
    const fuses = await getCurrentFuses({ app: appPath });

    assert.equal(fuses.binaryPath, dllPath);
    assert.notEqual(fuses.binaryPath, exePath);
    assert.equal(fuses.runAsNode, true);
    assert.equal(fuses.embeddedAsarIntegrityValidation, false);
    assert.equal(fuses.onlyLoadAppFromAsar, false);
  });
});

test('flipFuses accepts a Windows app directory string target and updates lynxtron.dll', async () => {
  await withTempWindowsApp(1, async ({ appPath, dllPath, exePath }) => {
    await flipFuses(appPath, {
      version: FuseVersion.V1,
      [FuseV1Options.RunAsNode]: false,
      [FuseV1Options.OnlyLoadAppFromAsar]: true,
    });

    const fuses = await getCurrentFuses(appPath);
    assert.equal(fuses.binaryPath, dllPath);
    assert.equal(fuses.runAsNode, false);
    assert.equal(fuses.embeddedAsarIntegrityValidation, false);
    assert.equal(fuses.onlyLoadAppFromAsar, true);

    const exeContents = await fs.readFile(exePath, 'utf8');
    assert.equal(exeContents, 'plain-exe-without-fuse-wire');
  });
});

test('getCurrentFuses resolves a macOS app bundle to the framework binary', async () => {
  await withTempMacApp(1, async ({ appPath, frameworkBinaryPath, appBinaryPath }) => {
    const fuses = await getCurrentFuses({ app: appPath });

    assert.equal(fuses.binaryPath, frameworkBinaryPath);
    assert.notEqual(fuses.binaryPath, appBinaryPath);
    assert.equal(fuses.runAsNode, true);
    assert.equal(fuses.embeddedAsarIntegrityValidation, false);
  });
});

test('flipFuses accepts a macOS app bundle string target and updates the framework binary', async () => {
  await withTempMacApp(1, async ({ appPath, frameworkBinaryPath, appBinaryPath }) => {
    await flipFuses(appPath, {
      version: FuseVersion.V1,
      [FuseV1Options.EnableNodeOptionsEnvironmentVariable]: false,
    });

    const fuses = await getCurrentFuses(appPath);
    assert.equal(fuses.binaryPath, frameworkBinaryPath);
    assert.equal(fuses.nodeOptions, false);

    const appBinaryContents = await fs.readFile(appBinaryPath, 'utf8');
    assert.equal(appBinaryContents, 'plain-mac-launcher-without-fuse-wire');
  });
});

test('flipFuses can enable embedded ASAR integrity validation', async () => {
  await withTempBinary(1, async (binaryPath) => {
    await flipFuses({ binary: binaryPath }, {
      version: FuseVersion.V1,
      [FuseV1Options.EnableEmbeddedAsarIntegrityValidation]: true,
    });

    const fuses = await getCurrentFuses({ binary: binaryPath });
    assert.equal(fuses.embeddedAsarIntegrityValidation, true);
    assert.equal(fuses.onlyLoadAppFromAsar, false);
  });
});
