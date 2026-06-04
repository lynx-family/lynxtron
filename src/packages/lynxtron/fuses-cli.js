#!/usr/bin/env node

import {
  flipFuses,
  FuseV1Options,
  FuseVersion,
  getCurrentFuses,
} from './fuses.js';

function printUsage() {
  console.log(`Usage:
  lynxtron-fuses read [--app <path>] [--binary <path>] [--json]
  lynxtron-fuses write [--app <path>] [--binary <path>] <fuse=on|off>...

--app prefers the macOS framework binary or Windows lynxtron.dll when given an app path.

Supported fuses:
  ${FuseV1Options.RunAsNode}
  ${FuseV1Options.EnableNodeOptionsEnvironmentVariable}
  ${FuseV1Options.EnableNodeCliInspectArguments}
  ${FuseV1Options.EnableEmbeddedAsarIntegrityValidation}
  ${FuseV1Options.OnlyLoadAppFromAsar}`);
}

function parseTargetAndFlags(args) {
  const target = {};
  const rest = [];
  let json = false;

  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === '--app' || arg === '--binary') {
      const value = args[index + 1];
      if (!value) {
        throw new Error(`Missing value for ${arg}`);
      }
      target[arg.slice(2)] = value;
      index += 1;
      continue;
    }

    if (arg === '--json') {
      json = true;
      continue;
    }

    rest.push(arg);
  }

  return { target, json, rest };
}

function parseFuseValue(rawValue) {
  switch (rawValue) {
    case '1':
    case 'on':
    case 'true':
    case 'enabled':
      return true;
    case '0':
    case 'off':
    case 'false':
    case 'disabled':
      return false;
    default:
      throw new Error(`Unsupported fuse value "${rawValue}"`);
  }
}

function parseAssignments(args) {
  const config = { version: FuseVersion.V1 };

  for (const arg of args) {
    const separatorIndex = arg.indexOf('=');
    if (separatorIndex === -1) {
      throw new Error(`Expected <fuse=value>, got "${arg}"`);
    }

    const key = arg.slice(0, separatorIndex);
    const rawValue = arg.slice(separatorIndex + 1).toLowerCase();
    config[key] = parseFuseValue(rawValue);
  }

  return config;
}

function formatFuses(fuses) {
  return [
    `binaryPath: ${fuses.binaryPath}`,
    `version: ${fuses.version}`,
    `${FuseV1Options.RunAsNode}: ${fuses.runAsNode ? 'on' : 'off'}`,
    `${FuseV1Options.EnableNodeOptionsEnvironmentVariable}: ${fuses.nodeOptions ? 'on' : 'off'}`,
    `${FuseV1Options.EnableNodeCliInspectArguments}: ${fuses.nodeCliInspect ? 'on' : 'off'}`,
    `${FuseV1Options.EnableEmbeddedAsarIntegrityValidation}: ${fuses.embeddedAsarIntegrityValidation ? 'on' : 'off'}`,
    `${FuseV1Options.OnlyLoadAppFromAsar}: ${fuses.onlyLoadAppFromAsar ? 'on' : 'off'}`,
  ].join('\n');
}

function getResignHint(binaryPath) {
  const normalizedBinaryPath = binaryPath.toLowerCase();
  if (normalizedBinaryPath.endsWith('.exe')) {
    return 'Fuse bytes were updated. Re-sign the Windows executable before distributing it.';
  }

  return 'Fuse bytes were updated. Re-sign the app or binary before distributing it.';
}

async function main() {
  const [command, ...args] = process.argv.slice(2);

  if (!command || command === '--help' || command === '-h') {
    printUsage();
    return;
  }

  if (command !== 'read' && command !== 'write') {
    throw new Error(`Unknown command "${command}"`);
  }

  const { target, json, rest } = parseTargetAndFlags(args);

  if (command === 'read') {
    const fuses = await getCurrentFuses(target);
    console.log(json ? JSON.stringify(fuses, null, 2) : formatFuses(fuses));
    return;
  }

  if (rest.length === 0) {
    throw new Error('write requires at least one <fuse=on|off> assignment');
  }

  const fuses = await flipFuses(target, parseAssignments(rest));
  console.log(json ? JSON.stringify(fuses, null, 2) : formatFuses(fuses));

  if (process.platform === 'darwin' || fuses.binaryPath.toLowerCase().endsWith('.exe')) {
    console.error(getResignHint(fuses.binaryPath));
  }
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : error);
  process.exit(1);
});
