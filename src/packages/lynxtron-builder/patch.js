#!/usr/bin/env node
'use strict';

// Runs as `prepack`: creates private, patched package copies under vendor/.
// Consumer installs never patch node_modules or a package-manager store.
const { execFileSync } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const Module = require('module');
const os = require('os');
const path = require('path');
const packageJson = require('./package.json');

const packageDir = __dirname;
const patchesDir = path.join(packageDir, 'patches');
const vendorDir = path.join(packageDir, 'vendor');
const markerPath = path.join(vendorDir, '.lynxtron-builder-patch.json');
const patchedPackages = packageJson.lynxtronPatchedDependencies;

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function getPatchesHash() {
  const hash = crypto.createHash('sha256');
  for (const name of fs.readdirSync(patchesDir).sort()) {
    hash.update(name);
    hash.update(fs.readFileSync(path.join(patchesDir, name)));
  }
  return hash.digest('hex');
}

function getBundleMetadata() {
  return {
    formatVersion: 1,
    patchHash: getPatchesHash(),
    packages: patchedPackages,
  };
}

function resolveElectronBuilderDir() {
  return path.dirname(
    require.resolve('electron-builder/package.json', { paths: [packageDir] })
  );
}

function resolveInstalledPackageDir(name) {
  const electronBuilderRequire = Module.createRequire(
    path.join(resolveElectronBuilderDir(), 'package.json')
  );
  const packageJsonPath = electronBuilderRequire.resolve(`${name}/package.json`);
  return fs.realpathSync(path.dirname(packageJsonPath));
}

function getVendoredPackageDir(name) {
  return path.join(vendorDir, name);
}

function verifyBundle() {
  const expected = getBundleMetadata();
  let actual;
  try {
    actual = readJson(markerPath);
  } catch {
    throw new Error(
      `@lynx-js/lynxtron-builder is missing its private electron-builder packages in ${vendorDir}. ` +
        'Reinstall the published package.'
    );
  }

  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(
      '@lynx-js/lynxtron-builder contains private packages that do not match its patches.'
    );
  }

  for (const [name, expectedVersion] of Object.entries(expected.packages)) {
    const manifestPath = path.join(getVendoredPackageDir(name), 'package.json');
    if (!fs.existsSync(manifestPath)) {
      throw new Error(`Missing private package ${name} at ${manifestPath}.`);
    }
    const actualVersion = readJson(manifestPath).version;
    if (actualVersion !== expectedVersion) {
      throw new Error(
        `Expected private ${name}@${expectedVersion}, found ${actualVersion}.`
      );
    }
  }
}

function copyPackage(source, destination) {
  fs.cpSync(source, destination, {
    recursive: true,
    dereference: true,
    // Source maps and typings are unused at runtime and maps no longer match patched code.
    filter: (src) => {
      const relative = path.relative(source, src);
      return (
        !relative.split(path.sep).includes('node_modules') &&
        !src.endsWith('.map') &&
        !src.endsWith('.d.ts')
      );
    },
  });
}

function bundle() {
  const stagingDir = fs.mkdtempSync(
    path.join(os.tmpdir(), 'lynxtron-builder-prepack-')
  );
  const stagingNodeModules = path.join(stagingDir, 'node_modules');
  const names = Object.keys(patchedPackages);

  try {
    fs.mkdirSync(stagingNodeModules, { recursive: true });
    fs.writeFileSync(
      path.join(stagingDir, 'package.json'),
      `${JSON.stringify({ private: true }, null, 2)}\n`
    );
    for (const name of names) {
      const source = resolveInstalledPackageDir(name);
      const actualVersion = readJson(path.join(source, 'package.json')).version;
      const expectedVersion = patchedPackages[name];
      if (actualVersion !== expectedVersion) {
        throw new Error(
          `Expected ${name}@${expectedVersion}, found ${actualVersion} at ${source}.`
        );
      }
      copyPackage(source, path.join(stagingNodeModules, name));
    }

    fs.cpSync(patchesDir, path.join(stagingDir, 'patches'), { recursive: true });
    execFileSync(
      process.execPath,
      [
        require.resolve('patch-package/dist/index.js'),
        '--patch-dir',
        'patches',
        '--error-on-fail',
      ],
      { cwd: stagingDir, stdio: 'inherit' }
    );

    fs.rmSync(vendorDir, { recursive: true, force: true });
    fs.mkdirSync(vendorDir, { recursive: true });
    // app-builder-lib and dmg-builder ship without a license file; redistribute
    // the modified copies with electron-builder's MIT license.
    const licensePath = path.join(resolveElectronBuilderDir(), 'LICENSE');
    for (const name of names) {
      fs.cpSync(
        path.join(stagingNodeModules, name),
        getVendoredPackageDir(name),
        { recursive: true }
      );
      fs.copyFileSync(licensePath, path.join(getVendoredPackageDir(name), 'LICENSE'));
    }
    fs.writeFileSync(markerPath, `${JSON.stringify(getBundleMetadata(), null, 2)}\n`);
    verifyBundle();
  } finally {
    fs.rmSync(stagingDir, { recursive: true, force: true });
  }
}

if (require.main === module) {
  bundle();
}

// Called once from the single-process build entry (cli.js). Published installs
// ship vendor/ and only need verification; source checkouts rebuild it on demand.
// Kept out of module-resolver.js so the many processes that load the resolver do
// not race to delete and rebuild the same vendor/ directory.
function ensureBundleForBuild() {
  try {
    verifyBundle();
  } catch (error) {
    if (packageDir.split(path.sep).includes('node_modules')) {
      throw error;
    }
    bundle();
  }
}

module.exports = {
  ensureBundleForBuild,
  getVendoredPackageDir,
  verifyBundle,
};
