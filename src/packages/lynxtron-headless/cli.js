#!/usr/bin/env node

import { recordBundle, runBundle, runReplay } from './lib/index.js';

const EXIT_CLI_CONFIG_ERROR = 5;

function printUsage() {
  console.error(`Usage:
  lynxtron-headless run <bundle-or-url> [options]
  lynxtron-headless record <bundle-or-url> [options]
  lynxtron-headless replay <replay.json> [options]

Options:
  --runtime <path>          Lynxtron executable path
  --artifact-dir <path>     Artifact directory
  --screenshot <path>       Screenshot PNG path
  --ui-dump <path>          UI dump JSON path
  --ui-dump-after-tap <path> UI dump JSON path after tap
  --report <path>           report.json path
  --trace <path>            trace.jsonl path
  --data <path>             JSON initial data path
  --global-props <path>     JSON globalProps path
  --width <number>          Viewport width
  --height <number>         Viewport height
  --dpr <number>            Device pixel ratio
  --tap <x,y>               Dispatch a tap after first screen
  --tap-text <text>         Find text in UI dump and tap its containing node
  --headed                  Show an authoring window while automation runs
  --slow-mo <ms>            Delay after each CDP action for visual playback
  --record-duration <ms>    Duration for headed input recording
  --allow-empty-recording   Do not fail when record exits with no input
  --smoke <name>            Smoke mode, for example "complex"
  --cdp-port <number>       CDP endpoint port, defaults to random free port
  --timeout <ms>            First-screen timeout
  --mode <semantic|exact|recorded> Replay mode, defaults to manifest default`);
}

function readOption(args, index) {
  const arg = args[index];
  const eq = arg.indexOf('=');
  if (eq !== -1) {
    return [arg.slice(0, eq), arg.slice(eq + 1), index];
  }
  return [arg, args[index + 1], index + 1];
}

function parseRunArgs(args) {
  const [bundle, ...rest] = args;
  if (!bundle) {
    throw new Error('Missing bundle-or-url');
  }

  const options = {};
  for (let i = 0; i < rest.length; i += 1) {
    const arg = rest[i];
    if (!arg.startsWith('--')) {
      throw new Error(`Unexpected argument: ${arg}`);
    }
    if (arg === '--headed') {
      options.headed = true;
      continue;
    }
    if (arg === '--allow-empty-recording') {
      options.allowEmptyRecording = true;
      continue;
    }
    const [name, value, nextIndex] = readOption(rest, i);
    i = nextIndex;
    if (value == null || value.startsWith('--')) {
      throw new Error(`Missing value for ${name}`);
    }
    switch (name) {
      case '--runtime':
        options.runtimeBinary = value;
        break;
      case '--artifact-dir':
        options.artifactDir = value;
        break;
      case '--screenshot':
        options.screenshot = value;
        break;
      case '--ui-dump':
        options.uiDump = value;
        break;
      case '--ui-dump-after-tap':
        options.uiDumpAfterTap = value;
        break;
      case '--report':
        options.report = value;
        break;
      case '--trace':
        options.trace = value;
        break;
      case '--data':
        options.data = value;
        break;
      case '--global-props':
        options.globalProps = value;
        break;
      case '--width':
        options.width = Number(value);
        break;
      case '--height':
        options.height = Number(value);
        break;
      case '--dpr':
        options.dpr = Number(value);
        break;
      case '--timeout':
        options.timeoutMs = Number(value);
        break;
      case '--headed':
        options.headed = value !== 'false';
        break;
      case '--slow-mo':
        options.slowMo = Number(value);
        break;
      case '--record-duration':
        options.recordDurationMs = Number(value);
        break;
      case '--allow-empty-recording':
        options.allowEmptyRecording = value !== 'false';
        break;
      case '--smoke':
        options.smoke = value;
        break;
      case '--cdp-port':
        options.cdpPort = Number(value);
        break;
      case '--mode':
        options.mode = value;
        break;
      case '--tap': {
        const [x, y] = value.split(',').map(Number);
        if (!Number.isFinite(x) || !Number.isFinite(y)) {
          throw new Error('--tap must use the form x,y');
        }
        options.tap = { x, y };
        break;
      }
      case '--tap-text':
        options.tapText = value;
        break;
      default:
        throw new Error(`Unknown option: ${name}`);
    }
  }
  return { bundle, options };
}

async function main() {
  const [command, ...args] = process.argv.slice(2);
  if (command !== 'run' && command !== 'record' && command !== 'replay') {
    printUsage();
    process.exit(EXIT_CLI_CONFIG_ERROR);
  }

  try {
    let result;
    if (command === 'run') {
      const { bundle, options } = parseRunArgs(args);
      result = await runBundle(bundle, {
        stdio: 'inherit',
        ...options,
      });
    } else if (command === 'record') {
      const { bundle, options } = parseRunArgs(args);
      result = await recordBundle(bundle, {
        stdio: 'inherit',
        ...options,
      });
    } else {
      const { bundle: manifestPath, options } = parseRunArgs(args);
      result = await runReplay(manifestPath, {
        stdio: 'inherit',
        ...options,
      });
    }
    process.exit(result.exitCode);
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    printUsage();
    process.exit(EXIT_CLI_CONFIG_ERROR);
  }
}

await main();
