#!/usr/bin/env node

const chalk = require('chalk');
const { hashElement } = require('folder-hash');
const minimist = require('minimist');

const childProcess = require('node:child_process');
const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const unknownFlags = [];

const pass = chalk.green('✓');
const fail = chalk.red('✗');

const FAILURE_STATUS_KEY = 'Electron_Spec_Runner_Failures';

const args = minimist(process.argv, {
  string: ['runners', 'target'],
  unknown: arg => unknownFlags.push(arg)
});

const unknownArgs = [];
for (const flag of unknownFlags) {
  unknownArgs.push(flag);
  const onlyFlag = flag.replace(/^-+/, '');
  if (args[onlyFlag]) {
    unknownArgs.push(args[onlyFlag]);
  }
}

const utils = require('./lib/utils');
const { YARN_VERSION, getYarnCommand } = require('./yarn');

const BASE = utils.SRC_DIR;

const runners = new Map([
  ['main', { description: 'Main process specs', run: runMainProcessElectronTests }]
]);

const specHashPath = path.resolve(__dirname, '../spec/.hash');

let runnersToRun = null;
if (args.runners !== undefined) {
  runnersToRun = args.runners.split(',').filter(value => value);
  if (!runnersToRun.every(r => [...runners.keys()].includes(r))) {
    console.log(`${fail} ${runnersToRun} must be a subset of [${[...runners.keys()].join(' | ')}]`);
    process.exit(1);
  }
  console.log('Only running:', runnersToRun);
} else {
  console.log(`Triggering runners: ${[...runners.keys()].join(', ')}`);
}

async function main () {
  const [lastSpecHash, lastSpecInstallHash] = loadLastSpecHash();
  const [currentSpecHash, currentSpecInstallHash] = await getSpecHash();
  const somethingChanged = (currentSpecHash !== lastSpecHash) ||
      (lastSpecInstallHash !== currentSpecInstallHash);

  if (somethingChanged) {
    await installSpecModules(path.resolve(__dirname, '..', 'spec'));
    await getSpecHash().then(saveSpecHash);
  }
  await buildLynxCards();
  await runLynxtronTests();
}

async function buildLynxCards () {
  const lynxCardDir = path.resolve(__dirname, '..', 'spec', 'case', 'lynx-card');
  const { entries } = require(path.join(lynxCardDir, 'cards.config'));
  const distDir = path.join(lynxCardDir, 'dist');
  const allBuilt = Object.keys(entries).every(name =>
    fs.existsSync(path.join(distDir, `${name}.lynx.bundle`))
  );
  if (allBuilt) return;

  console.info('\nBuilding lynx-card test bundles...');
  const { status } = childProcess.spawnSync('node', ['./build-all.js'], {
    cwd: lynxCardDir,
    stdio: 'inherit',
    shell: process.platform === 'win32'
  });
  if (status !== 0) {
    console.log(`${fail} Failed to build lynx-card test bundles`);
    process.exit(1);
  }
}

function loadLastSpecHash () {
  return fs.existsSync(specHashPath)
    ? fs.readFileSync(specHashPath, 'utf8').split('\n')
    : [null, null];
}

function saveSpecHash ([newSpecHash, newSpecInstallHash]) {
  fs.writeFileSync(specHashPath, `${newSpecHash}\n${newSpecInstallHash}`);
}

async function runLynxtronTests() {
  const errors = [];

  const testResultsDir = process.env.ELECTRON_TEST_RESULTS_DIR;
  for (const [runnerId, { description, run }] of runners) {
    if (runnersToRun && !runnersToRun.includes(runnerId)) {
      console.info('\nSkipping:', description);
      continue;
    }
    try {
      console.info('\nRunning:', description);
      if (testResultsDir) {
        process.env.MOCHA_FILE = path.join(testResultsDir, `test-results-${runnerId}.xml`);
      }
      await run();
    } catch (err) {
      errors.push([runnerId, err]);
    }
  }

  if (errors.length !== 0) {
    for (const err of errors) {
      console.error('\n\nRunner Failed:', err[0]);
      console.error(err[1]);
    }
    console.log(`${fail} Electron test runners have failed`);
    process.exit(1);
  }
}

async function asyncSpawn (exe, runnerArgs) {
  return new Promise((resolve, reject) => {
    let forceExitResult = 0;
    const child = childProcess.spawn(exe, runnerArgs, {
      cwd: path.resolve(__dirname, '../..')
    });
    if (process.env.ELECTRON_TEST_PID_DUMP_PATH && child.pid) {
      fs.writeFileSync(process.env.ELECTRON_TEST_PID_DUMP_PATH, child.pid.toString());
    }
    child.stdout.pipe(process.stdout);
    child.stderr.pipe(process.stderr);
    if (process.env.ELECTRON_FORCE_TEST_SUITE_EXIT) {
      child.stdout.on('data', data => {
        const failureRE = RegExp(`${FAILURE_STATUS_KEY}: (\\d.*)`);
        const failures = data.toString().match(failureRE);
        if (failures) {
          forceExitResult = parseInt(failures[1], 10);
        }
      });
    }
    child.on('error', error => reject(error));
    child.on('close', (status, signal) => {
      let returnStatus = 0;
      if (process.env.ELECTRON_FORCE_TEST_SUITE_EXIT) {
        returnStatus = forceExitResult;
      } else {
        returnStatus = status;
      }
      resolve({ status: returnStatus, signal });
    });
  });
}

async function runTestUsingElectron (specDir, testName) {
  let exe;
  exe = path.resolve(BASE, utils.getElectronExec());
  const runnerArgs = [path.resolve(__dirname, '..', specDir), ...unknownArgs.slice(2)];
  if (process.platform === 'linux') {
    runnerArgs.unshift(path.resolve(__dirname, 'dbus_mock.py'), exe);
    exe = 'python3';
  }
  const { status, signal } = await asyncSpawn(exe, runnerArgs);
  if (status !== 0) {
    if (status) {
      const textStatus = process.platform === 'win32' ? `0x${status.toString(16)}` : status.toString();
      console.log(`${fail} Electron tests failed with code ${textStatus}.`);
    } else {
      console.log(`${fail} Electron tests failed with kill signal ${signal}.`);
    }
    process.exit(1);
  }
  console.log(`${pass} Electron ${testName} process tests passed.`);
}

async function runMainProcessElectronTests () {
  await runTestUsingElectron('spec', 'main');
}

async function installSpecModules (dir) {
  const env = {
    npm_config_msvs_version: '2022',
    ...process.env,
    CXXFLAGS: process.env.CXXFLAGS,
    npm_config_yes: 'true'
  };
  env.npm_config_nodedir = path.resolve(BASE, `out/${utils.getOutDir({ shouldLog: true })}/gen/node_headers`);
  if (fs.existsSync(path.resolve(dir, 'node_modules'))) {
    await fs.promises.rm(path.resolve(dir, 'node_modules'), { force: true, recursive: true });
  }
  const [cmd, args] = getYarnCommand(YARN_VERSION);
  const { status } = childProcess.spawnSync(cmd, [...args, 'install', '--immutable'], {
    env,
    cwd: dir,
    stdio: 'inherit',
    shell: process.platform === 'win32'
  });
  if (status !== 0 && !process.env.IGNORE_YARN_INSTALL_ERROR) {
    console.log(`${fail} Failed to yarn install in '${dir}'`);
    process.exit(1);
  }
}

function getSpecHash () {
  return Promise.all([
    (async () => {
      const hasher = crypto.createHash('SHA256');
      hasher.update(fs.readFileSync(path.resolve(__dirname, '../spec/package.json')));
      hasher.update(fs.readFileSync(path.resolve(__dirname, '../spec/yarn.lock')));
      hasher.update(fs.readFileSync(path.resolve(__dirname, '../script/spec-runner.js')));
      return hasher.digest('hex');
    })(),
    (async () => {
      const specNodeModulesPath = path.resolve(__dirname, '../spec/node_modules');
      if (!fs.existsSync(specNodeModulesPath)) {
        return null;
      }
      const { hash } = await hashElement(specNodeModulesPath, {
        folders: {
          exclude: ['.bin']
        }
      });
      return hash;
    })()
  ]);
}

main().catch((error) => {
  console.error('An error occurred inside the spec runner:', error);
  process.exit(1);
});
