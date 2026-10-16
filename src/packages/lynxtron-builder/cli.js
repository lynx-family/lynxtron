#!/usr/bin/env node
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');
const { makeUniversalApp } = require('@electron/universal');
const {
  applyArchitectureToConfig,
  prepareRuntimeConfig,
  prepareUniversalPackaging,
  resolveBuildPlan,
  resolveCliArchitectures,
  resolveTargetPlatform,
} = require('./runtime-config.js');
const { prepareAutoLinkPackaging } = require('./autolink-packaging.js');
const { ensureBundleForBuild } = require('./patch.js');

const projectRoot = process.cwd();
const configPath = path.join(projectRoot, 'electron-builder.yml');
const tempConfigPath = path.join(projectRoot, 'config.json');
// Preload via execArgv so processes forked by electron-builder inherit it.
const electronBuilderArgs = [
  '--require',
  path.join(__dirname, 'module-resolver.js'),
  path.join(__dirname, 'electron-builder-launcher.js'),
];

function getLynxtronPackage() {
  try {
    const packageJsonPath = require.resolve('@lynx-js/lynxtron/package.json', {
      paths: [projectRoot],
    });
    const runtimeArtifactsPath = require.resolve(
      '@lynx-js/lynxtron/runtime-artifacts',
      { paths: [projectRoot] }
    );
    const packageJson = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
    if (packageJson.version) {
      return {
        version: packageJson.version,
        runtimeDistribution: packageJson.lynxtronRuntimeDistribution,
        runtimeArtifacts: require(runtimeArtifactsPath),
      };
    }
  } catch (e) {
    console.warn('Could not resolve the installed Lynxtron package.', e.message);
  }

  throw new Error('Failed to determine Lynxtron version. Please check package.json and node_modules.');
}

async function build() {
  // Single-process entry: make sure the patched vendor/ exists before any
  // electron-builder subprocess loads the resolver (which only verifies it).
  ensureBundleForBuild();
  const rawArgs = process.argv.slice(2);
  const { runtimeArtifacts } = getLynxtronPackage();
  const { forwardedArgs } = runtimeArtifacts.parseRuntimeArguments(rawArgs);

  const config = readConfig();
  const targetPlatform = resolveTargetPlatform(forwardedArgs, process.platform);
  const buildPlan = resolveBuildPlan(forwardedArgs, config, targetPlatform);

  try {
    if (buildPlan.universal) {
      await runBuild('--x64', rawArgs, { filterTargets: true });
      await runBuild('--arm64', rawArgs, { filterTargets: true });
      await makeUniversal();
    } else if (buildPlan.architectures.length > 1) {
      for (const architecture of buildPlan.architectures) {
        await runBuild(`--${architecture}`, rawArgs, { filterTargets: true });
      }
    } else {
      await runBuild(undefined, rawArgs);
    }
  } finally {
    if (fs.existsSync(tempConfigPath)) {
      fs.unlinkSync(tempConfigPath);
    }
  }
}

function readConfig() {
  if (!fs.existsSync(configPath)) {
    return {};
  }
  return yaml.load(fs.readFileSync(configPath, 'utf8')) || {};
}

async function runBuild(arch, rawArgs, { filterTargets = false } = {}) {
  const config = readConfig();
  const lynxtronPackage = getLynxtronPackage();
  const prepared = prepareRuntimeConfig({
    config,
    args: rawArgs,
    env: process.env,
    arch,
    platform: process.platform,
    defaultArch: process.arch,
    lynxtronVersion: lynxtronPackage.version,
    runtimeDistribution: lynxtronPackage.runtimeDistribution,
    runtimeArtifacts: lynxtronPackage.runtimeArtifacts,
  });

  if (prepared.useNpmRuntime) {
    const managerPath = require.resolve('@lynx-js/lynxtron/runtime-manager', { paths: [projectRoot] });
    const { ensureTargetRuntime } = await import(require('node:url').pathToFileURL(managerPath).href);
    config.electronDist = await ensureTargetRuntime({
      platform: prepared.platform, arch: prepared.arch, version: prepared.version,
      variant: prepared.variant, cwd: projectRoot,
    });
  }

  const cliArchitectures = resolveCliArchitectures(prepared.forwardedArgs);
  if (arch || cliArchitectures.length === 1) {
    applyArchitectureToConfig(config, prepared.platform, prepared.arch, {
      filterTargets,
    });
  }

  prepareAutoLinkPackaging({
    config,
    projectRoot,
    platform: prepared.platform,
    arch: prepared.arch,
  });

  fs.writeFileSync(tempConfigPath, JSON.stringify(config, null, 2));

  const args = prepared.forwardedArgs.filter(arg =>
    arg !== '--universal' &&
    arg !== '--mas' &&
    (!arch || !resolveCliArchitectures([arg]).length)
  );
  const finalArgs = ['-c', tempConfigPath, ...args];
  if (arch) {
    finalArgs.push(arch);
  }
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [...electronBuilderArgs, ...finalArgs], {
      stdio: 'inherit',
    });

    child.on('error', reject);
    child.on('close', (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`Build failed with code ${code}`));
      }
    });
  });
}

async function makeUniversal() {
  let config = {};
  if (fs.existsSync(configPath)) {
    const yamlContent = fs.readFileSync(configPath, 'utf8');
    config = yaml.load(yamlContent);
  }

  const x64AppPath = path.resolve(projectRoot, config.directories.output, 'mac', `${config.productName}.app`);
  const arm64AppPath = path.resolve(projectRoot, config.directories.output, 'mac-arm64', `${config.productName}.app`);
  const outAppPath = path.resolve(projectRoot, config.directories.output, 'mac-universal', `${config.productName}.app`);

  await makeUniversalApp({
    x64AppPath,
    arm64AppPath,
    outAppPath,
  });

  const packaging = prepareUniversalPackaging({
    config,
    configPath: tempConfigPath,
    outAppPath,
  });
  fs.writeFileSync(tempConfigPath, JSON.stringify(packaging.config, null, 2));

  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [...electronBuilderArgs, ...packaging.args], {
      stdio: 'inherit',
    });

    child.on('error', reject);
    child.on('close', (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`Universal packaging failed with code ${code}`));
      }
    });
  });
}

build().catch(err => {
  console.error(err);
  process.exit(1);
});
