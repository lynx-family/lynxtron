#!/usr/bin/env node
'use strict';

// Publish-time preparation for @lynx-js/lynxtron-builder.
//
// The patched toolchain is shipped as ordinary files under vendor/node_modules.
// Yarn removes a package's top-level bundled node_modules during installation;
// keeping this tree under vendor/ preserves the patches with both Yarn and pnpm.
// Consumers do not need to patch their installed dependencies.
//
// Steps:
//   1. Install the toolchain into ./vendor/node_modules.
//   2. Apply the checked-in patches with patch-package.
//   3. Remove the publish-only patching tool and verify the resulting tree.

const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const packagePath = __dirname;
const vendorPath = path.join(packagePath, 'vendor');

// Marker strings that only exist AFTER the corresponding patch is applied.
// Keep these in sync with patches/*.patch; a missing marker aborts publishing.
const PATCH_VERIFICATIONS = [
  {
    file: 'node_modules/app-builder-lib/out/electron/electronVersion.js',
    marker: 'const electronPackages = ["lynxtron"];',
  },
  {
    file: 'node_modules/dmg-builder/out/dmgUtil.js',
    marker: 'use image size as default size if user did not set window size',
  },
];

function run(command, args) {
  console.log(`[prepare-bundled-deps] $ ${command} ${args.join(' ')}`);
  execFileSync(command, args, { cwd: vendorPath, stdio: 'inherit' });
}

function runNpm(args) {
  if (process.platform === 'win32') {
    // npm.cmd must be launched through the Windows command interpreter.
    run(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', 'npm', ...args]);
  } else {
    run('npm', args);
  }
}

function main() {
  const manifest = JSON.parse(fs.readFileSync(path.join(packagePath, 'package.json'), 'utf8'));
  const dependencies = {};
  for (const name of [
    'app-builder-lib', 'dmg-builder', 'electron-builder', 'electron-builder-squirrel-windows',
  ]) {
    dependencies[name] = manifest.devDependencies[name];
  }
  fs.rmSync(vendorPath, { recursive: true, force: true });
  fs.mkdirSync(vendorPath, { recursive: true });
  fs.writeFileSync(path.join(vendorPath, 'package.json'), JSON.stringify({
    private: true,
    dependencies,
    devDependencies: { 'patch-package': manifest.devDependencies['patch-package'] },
  }, null, 2) + '\n');

  // Isolate npm from the parent Yarn workspaces. Include macOS-only DMG
  // dependencies even on other publishing hosts so the shipped tree is complete.
  const npmArgs = [
    '--workspaces=false', '--include=optional', '--os=darwin', '--no-audit', '--no-fund',
  ];
  runNpm(['install', '--include=dev', ...npmArgs]);

  // 2. Apply the checked-in patches with the installed tool, failing on any
  //    rejected hunk rather than relying only on the marker checks below.
  run(process.execPath, [
    path.join(vendorPath, 'node_modules', 'patch-package', 'index.js'),
    '--patch-dir', '../patches', '--error-on-fail',
  ]);

  runNpm(['prune', '--omit=dev', '--ignore-scripts', ...npmArgs]);
  fs.rmSync(path.join(vendorPath, 'package-lock.json'), { force: true });
  fs.rmSync(path.join(vendorPath, 'node_modules/.package-lock.json'), { force: true });

  // 3. Verify every patch actually landed before publishing.
  for (const { file, marker } of PATCH_VERIFICATIONS) {
    const target = path.join(vendorPath, file);
    if (!fs.existsSync(target)) {
      throw new Error(`[prepare-bundled-deps] Expected patched file missing: ${file}`);
    }
    const content = fs.readFileSync(target, 'utf8');
    if (!content.includes(marker)) {
      throw new Error(
        `[prepare-bundled-deps] Patch not applied to ${file}: marker not found. ` +
          `Check patches/ against the installed dependency version.`
      );
    }
    console.log(`[prepare-bundled-deps] verified patch applied: ${file}`);
  }

  // npm tolerates failed optional dependencies, so check the files needed by
  // DMG packaging explicitly before allowing an incomplete bundle to publish.
  for (const file of [
    'node_modules/dmg-license/package.json',
    'node_modules/iconv-corefoundation/lib/native.node',
  ]) {
    if (!fs.existsSync(path.join(vendorPath, file))) {
      throw new Error(`[prepare-bundled-deps] Required DMG dependency missing: ${file}`);
    }
  }

  console.log('[prepare-bundled-deps] bundled dependencies prepared and patched.');
}

try {
  main();
} catch (error) {
  console.error(error.message || error);
  process.exit(1);
}
