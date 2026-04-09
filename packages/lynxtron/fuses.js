import fs from 'node:fs/promises';
import path from 'node:path';

import lynxtronBinaryPath from './lynxtron_bin.js';

const FUSE_SENTINEL = Buffer.from('dL7pKGdnNz796PbbjQWNKmHXBZaB9tsX', 'ascii');
const FUSE_VERSION = 1;
const MAX_SUPPORTED_SENTINELS = 2;
const ENABLED_FUSE_VALUE = '1';
const DISABLED_FUSE_VALUE = '0';

const FUSE_SCHEMA = [
  { key: 'runAsNode', defaultValue: true },
  { key: 'nodeOptions', defaultValue: true },
  { key: 'nodeCliInspect', defaultValue: true },
  { key: 'embeddedAsarIntegrityValidation', defaultValue: false },
  { key: 'onlyLoadAppFromAsar', defaultValue: false },
];

export const FuseVersion = Object.freeze({
  V1: FUSE_VERSION,
});

export const FuseV1Options = Object.freeze({
  RunAsNode: 'runAsNode',
  EnableNodeOptionsEnvironmentVariable: 'nodeOptions',
  EnableNodeCliInspectArguments: 'nodeCliInspect',
  EnableEmbeddedAsarIntegrityValidation: 'embeddedAsarIntegrityValidation',
  OnlyLoadAppFromAsar: 'onlyLoadAppFromAsar',
});

function resolveTarget(target) {
  if (target == null) {
    return { binary: lynxtronBinaryPath };
  }

  if (typeof target === 'string') {
    const normalizedTarget = target.toLowerCase();
    if (normalizedTarget.endsWith('.app')) {
      return { app: target };
    }

    if (normalizedTarget.endsWith('.exe')) {
      return { binary: target };
    }

    return { app: target };
  }

  if (typeof target !== 'object') {
    throw new TypeError('target must be a path string or an options object');
  }

  return target;
}

async function pathExists(candidatePath) {
  try {
    await fs.access(candidatePath);
    return true;
  } catch {
    return false;
  }
}

async function resolveAppBinaryPath(appPath) {
  const resolvedAppPath = path.resolve(appPath);
  const lowerCaseAppPath = resolvedAppPath.toLowerCase();

  if (lowerCaseAppPath.endsWith('.app')) {
    const frameworkBinaryPath = path.join(
      resolvedAppPath,
      'Contents',
      'Frameworks',
      'Lynxtron Framework.framework',
      'Lynxtron Framework'
    );
    if (await pathExists(frameworkBinaryPath)) {
      return frameworkBinaryPath;
    }

    return path.join(resolvedAppPath, 'Contents', 'MacOS', 'Lynxtron');
  }

  if (lowerCaseAppPath.endsWith('.exe')) {
    return resolvedAppPath;
  }

  const macFrameworkBinaryPath = path.join(
    resolvedAppPath,
    'Contents',
    'Frameworks',
    'Lynxtron Framework.framework',
    'Lynxtron Framework'
  );
  if (await pathExists(macFrameworkBinaryPath)) {
    return macFrameworkBinaryPath;
  }

  const macBinaryPath = path.join(
    resolvedAppPath,
    'Contents',
    'MacOS',
    'Lynxtron'
  );
  if (await pathExists(macBinaryPath)) {
    return macBinaryPath;
  }

  for (const candidate of ['lynxtron.dll', 'Lynxtron.dll']) {
    const windowsBinaryPath = path.join(resolvedAppPath, candidate);
    if (await pathExists(windowsBinaryPath)) {
      return windowsBinaryPath;
    }
  }

  for (const candidate of ['lynxtron.exe', 'Lynxtron.exe']) {
    const windowsExecutablePath = path.join(resolvedAppPath, candidate);
    if (await pathExists(windowsExecutablePath)) {
      return windowsExecutablePath;
    }
  }

  throw new Error(
    `Unable to resolve a Lynxtron binary from app path: ${resolvedAppPath}`
  );
}

async function resolveBinaryPath(target) {
  if (target.binary && target.app) {
    throw new Error('Provide either "binary" or "app", not both');
  }

  if (target.binary) {
    return path.resolve(target.binary);
  }

  if (target.app) {
    return resolveAppBinaryPath(target.app);
  }

  return path.resolve(lynxtronBinaryPath);
}

function findSentinelOffsets(buffer) {
  const offsets = [];
  let searchFrom = 0;

  while (searchFrom < buffer.length) {
    const offset = buffer.indexOf(FUSE_SENTINEL, searchFrom);
    if (offset === -1) {
      break;
    }

    offsets.push(offset);
    searchFrom = offset + FUSE_SENTINEL.length;
  }

  if (offsets.length === 0) {
    throw new Error('Unable to locate a Lynxtron fuse wire in the target binary');
  }

  if (offsets.length > MAX_SUPPORTED_SENTINELS) {
    throw new Error(
      `Unsupported Lynxtron binary: found ${offsets.length} fuse sentinels`
    );
  }

  return offsets;
}

function getWireRecord(buffer, sentinelOffset) {
  const versionOffset = sentinelOffset + FUSE_SENTINEL.length;
  const wireLengthOffset = versionOffset + 1;
  const wireOffset = wireLengthOffset + 1;
  const version = buffer[versionOffset];
  const wireLength = buffer[wireLengthOffset];

  if (version !== FUSE_VERSION) {
    throw new Error(
      `Unsupported Lynxtron fuse version ${version}; expected ${FUSE_VERSION}`
    );
  }

  if (wireLength !== FUSE_SCHEMA.length) {
    throw new Error(
      `Unsupported Lynxtron fuse wire length ${wireLength}; expected ${FUSE_SCHEMA.length}`
    );
  }

  return {
    wireOffset,
    wireLength,
    wire: buffer.subarray(wireOffset, wireOffset + wireLength),
  };
}

function readWire(buffer) {
  const records = findSentinelOffsets(buffer).map((offset) =>
    getWireRecord(buffer, offset)
  );
  const referenceWire = Buffer.from(records[0].wire);

  for (const record of records.slice(1)) {
    if (!Buffer.from(record.wire).equals(referenceWire)) {
      throw new Error('Fuse wires differ across binary slices');
    }
  }

  return { records, wire: referenceWire };
}

function decodeWire(wire) {
  const values = { version: FuseVersion.V1 };

  for (const [index, fuse] of FUSE_SCHEMA.entries()) {
    const rawValue = String.fromCharCode(wire[index]);
    if (rawValue !== ENABLED_FUSE_VALUE && rawValue !== DISABLED_FUSE_VALUE) {
      throw new Error(`Unsupported fuse value "${rawValue}" for ${fuse.key}`);
    }

    values[fuse.key] = rawValue === ENABLED_FUSE_VALUE;
  }

  return values;
}

function encodeWire(currentFuses, config) {
  if (config.version !== FuseVersion.V1) {
    throw new Error(
      `Unsupported Lynxtron fuse version ${config.version}; expected ${FuseVersion.V1}`
    );
  }

  const nextFuses = { ...currentFuses };

  for (const [key, value] of Object.entries(config)) {
    if (key === 'version') {
      continue;
    }

    if (!Object.values(FuseV1Options).includes(key)) {
      throw new Error(`Unknown Lynxtron fuse "${key}"`);
    }

    if (typeof value !== 'boolean') {
      throw new TypeError(`Fuse "${key}" must be a boolean`);
    }

    nextFuses[key] = value;
  }

  const bytes = FUSE_SCHEMA.map(({ key }) =>
    nextFuses[key] ? ENABLED_FUSE_VALUE : DISABLED_FUSE_VALUE
  );

  return { nextFuses, wire: Buffer.from(bytes.join(''), 'ascii') };
}

export async function getCurrentFuses(target) {
  const binaryPath = await resolveBinaryPath(resolveTarget(target));
  const buffer = await fs.readFile(binaryPath);
  const { wire } = readWire(buffer);

  return {
    binaryPath,
    ...decodeWire(wire),
  };
}

export async function flipFuses(target, config) {
  if (config == null || typeof config !== 'object') {
    throw new TypeError('config must be an object');
  }

  const binaryPath = await resolveBinaryPath(resolveTarget(target));
  const buffer = await fs.readFile(binaryPath);
  const { records, wire: currentWire } = readWire(buffer);
  const currentFuses = decodeWire(currentWire);
  const { nextFuses, wire } = encodeWire(currentFuses, config);

  for (const record of records) {
    wire.copy(buffer, record.wireOffset);
  }

  await fs.writeFile(binaryPath, buffer);

  return {
    binaryPath,
    ...nextFuses,
  };
}

export function getFuseDefaults() {
  return FUSE_SCHEMA.reduce(
    (result, fuse) => ({ ...result, [fuse.key]: fuse.defaultValue }),
    { version: FuseVersion.V1 }
  );
}
