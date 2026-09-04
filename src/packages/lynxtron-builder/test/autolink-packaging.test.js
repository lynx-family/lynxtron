const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');
const {
  validateConfiguration,
} = require('app-builder-lib/out/util/config/config.js');

const { prepareAutoLinkPackaging } = require('../autolink-packaging.js');

const tempDirectories = [];

test.afterEach(() => {
  for (const directory of tempDirectories.splice(0)) {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('packages macOS AutoLink addons, Frameworks, and app bundles', async () => {
  const fixture = createFixture();
  const config = {
    directories: { app: 'dist/desktop' },
    files: ['main.js'],
    asar: true,
  };

  const result = prepareAutoLinkPackaging({
    config,
    projectRoot: fixture.projectRoot,
    platform: 'darwin',
    arch: 'arm64',
  });

  assert.equal(result.libraries.length, 1);
  assert.ok(config.files.includes('.lynxtron/native/**/*'));
  assert.ok(config.files.includes('!node_modules/@lynx-js/cef-webview'));
  assert.ok(
    config.files.includes('!node_modules/@lynx-js/cef-webview/**/*')
  );
  assert.ok(
    config.files.includes(
      '!.lynxtron/native/node_modules/@lynx-js/cef-webview/dist/darwin/arm64/frameworks/Chromium Embedded Framework.framework/**/*'
    )
  );
  assert.ok(
    config.files.includes(
      '!.lynxtron/native/node_modules/@lynx-js/cef-webview/dist/darwin/arm64/frameworks/LynxtronWebview Helper.app/**/*'
    )
  );
  assert.ok(config.asarUnpack.includes('.lynxtron/native/**/*'));
  assert.deepEqual(config.extraFiles, [
    {
      from: fixture.frameworksPath,
      to: 'Frameworks/Chromium Embedded Framework.framework',
      filter: ['**/*'],
    },
    {
      from: fixture.appBundlePath,
      to: 'Frameworks/LynxtronWebview Helper.app',
      filter: ['**/*'],
    },
  ]);

  const debugLogger = { isEnabled: false, add() {} };
  await assert.doesNotReject(validateConfiguration(config, debugLogger));
});

test('packages Windows AutoLink addon and adjacent CEF runtime files unpacked', () => {
  const fixture = createFixture();
  const config = {
    directories: { app: 'dist/desktop' },
    files: ['main.js'],
    asar: true,
  };

  const result = prepareAutoLinkPackaging({
    config,
    projectRoot: fixture.projectRoot,
    platform: 'win32',
    arch: 'x64',
  });

  assert.equal(result.libraries.length, 1);
  assert.equal(
    result.libraries[0].files[0].path,
    'dist/win32/x64/cef_extension.node'
  );
  assert.equal(result.libraries[0].files[1].path, 'dist/win32/x64/libcef.dll');
  assert.ok(config.files.includes('.lynxtron/native/**/*'));
  assert.ok(config.files.includes('!node_modules/@lynx-js/cef-webview'));
  assert.ok(
    config.files.includes('!node_modules/@lynx-js/cef-webview/**/*')
  );
  assert.ok(config.asarUnpack.includes('.lynxtron/native/**/*'));
  assert.equal(config.extraFiles, undefined);
  assert.ok(
    fs.existsSync(
      path.join(
        result.libraries[0].packageRoot,
        'dist',
        'win32',
        'x64',
        'libcef.dll'
      )
    )
  );
});

test('preserves electron-builder default app files when files is omitted', () => {
  const fixture = createFixture();
  const config = {
    directories: { app: 'dist/desktop' },
    asar: true,
  };

  prepareAutoLinkPackaging({
    config,
    projectRoot: fixture.projectRoot,
    platform: 'win32',
    arch: 'x64',
  });

  assert.deepEqual(config.files, [
    '**/*',
    '.lynxtron/native/**/*',
    '!node_modules/@lynx-js/cef-webview',
    '!node_modules/@lynx-js/cef-webview/**/*',
  ]);
});

test('fails before packaging when the target architecture artifact is absent', () => {
  const fixture = createFixture();
  fs.rmSync(
    path.join(
      fixture.packageRoot,
      'dist',
      'darwin',
      'x64',
      'cef_extension.node'
    )
  );

  assert.throws(
    () =>
      prepareAutoLinkPackaging({
        config: { directories: { app: 'dist/desktop' } },
        projectRoot: fixture.projectRoot,
        platform: 'darwin',
        arch: 'x64',
      }),
    /does not exist for darwin\/x64/
  );
});

test('rejects legacy Lynxtron artifact categories', () => {
  const fixture = createFixture();
  const manifestPath = path.join(fixture.packageRoot, 'lynx.lib.json');
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const target = manifest.platforms.lynxtron.targets.find(
    ({ os, arch }) => os === 'win32' && arch === 'x64'
  );
  target.binaries = target.files;
  delete target.files;
  fs.writeFileSync(manifestPath, JSON.stringify(manifest));

  assert.throws(
    () =>
      prepareAutoLinkPackaging({
        config: { directories: { app: 'dist/desktop' } },
        projectRoot: fixture.projectRoot,
        platform: 'win32',
        arch: 'x64',
      }),
    /does not support binary, binaries, or resources.*use files/
  );
});

test('rejects app bundles for non-macOS targets', () => {
  const fixture = createFixture();
  const manifestPath = path.join(fixture.packageRoot, 'lynx.lib.json');
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const target = manifest.platforms.lynxtron.targets.find(
    ({ os, arch }) => os === 'win32' && arch === 'x64'
  );
  target.appBundles = [
    'dist/darwin/arm64/frameworks/LynxtronWebview Helper.app',
  ];
  fs.writeFileSync(manifestPath, JSON.stringify(manifest));

  assert.throws(
    () =>
      prepareAutoLinkPackaging({
        config: { directories: { app: 'dist/desktop' } },
        projectRoot: fixture.projectRoot,
        platform: 'win32',
        arch: 'x64',
      }),
    /only supports appBundles for darwin targets/
  );
});

test('rejects Framework bundles for non-macOS targets', () => {
  const fixture = createFixture();
  const manifestPath = path.join(fixture.packageRoot, 'lynx.lib.json');
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const target = manifest.platforms.lynxtron.targets.find(
    ({ os, arch }) => os === 'win32' && arch === 'x64'
  );
  target.frameworks = [
    'dist/darwin/arm64/frameworks/Chromium Embedded Framework.framework',
  ];
  fs.writeFileSync(manifestPath, JSON.stringify(manifest));

  assert.throws(
    () =>
      prepareAutoLinkPackaging({
        config: { directories: { app: 'dist/desktop' } },
        projectRoot: fixture.projectRoot,
        platform: 'win32',
        arch: 'x64',
      }),
    /only supports frameworks for darwin targets/
  );
});

function createFixture() {
  const projectRoot = fs.mkdtempSync(
    path.join(os.tmpdir(), 'lynxtron-builder-autolink-')
  );
  tempDirectories.push(projectRoot);
  const packageRoot = path.join(
    projectRoot,
    'dist',
    'desktop',
    '.lynxtron',
    'native',
    'node_modules',
    '@lynx-js',
    'cef-webview'
  );
  const frameworksPath = path.join(
    packageRoot,
    'dist',
    'darwin',
    'arm64',
    'frameworks'
  );
  fs.mkdirSync(frameworksPath, { recursive: true });
  const frameworkPath = path.join(
    frameworksPath,
    'Chromium Embedded Framework.framework'
  );
  const appBundlePath = path.join(
    frameworksPath,
    'LynxtronWebview Helper.app'
  );
  fs.mkdirSync(frameworkPath, { recursive: true });
  fs.mkdirSync(path.join(appBundlePath, 'Contents', 'MacOS'), {
    recursive: true,
  });
  fs.mkdirSync(path.join(packageRoot, 'dist', 'darwin', 'x64'), {
    recursive: true,
  });
  fs.mkdirSync(path.join(packageRoot, 'dist', 'win32', 'x64'), {
    recursive: true,
  });
  fs.writeFileSync(
    path.join(packageRoot, 'package.json'),
    JSON.stringify({ name: '@lynx-js/cef-webview' })
  );
  fs.writeFileSync(path.join(packageRoot, 'index.cjs'), 'module.exports = {};');
  fs.writeFileSync(
    path.join(packageRoot, 'dist', 'darwin', 'arm64', 'cef_extension.node'),
    'arm64'
  );
  fs.writeFileSync(
    path.join(packageRoot, 'dist', 'darwin', 'x64', 'cef_extension.node'),
    'x64'
  );
  fs.writeFileSync(
    path.join(packageRoot, 'dist', 'win32', 'x64', 'cef_extension.node'),
    'win32'
  );
  fs.writeFileSync(
    path.join(packageRoot, 'dist', 'win32', 'x64', 'libcef.dll'),
    'cef'
  );
  fs.writeFileSync(
    path.join(frameworkPath, 'Chromium Embedded Framework'),
    'framework'
  );
  fs.writeFileSync(
    path.join(
      appBundlePath,
      'Contents',
      'MacOS',
      'LynxtronWebview Helper'
    ),
    'helper'
  );
  fs.writeFileSync(
    path.join(packageRoot, 'lynx.lib.json'),
    JSON.stringify({
      platforms: {
        lynxtron: {
          targets: [
            {
              os: 'darwin',
              arch: 'arm64',
              files: ['dist/darwin/arm64/cef_extension.node'],
              frameworks: [
                'dist/darwin/arm64/frameworks/Chromium Embedded Framework.framework',
              ],
              appBundles: [
                'dist/darwin/arm64/frameworks/LynxtronWebview Helper.app',
              ],
            },
            {
              os: 'darwin',
              arch: 'x64',
              files: ['dist/darwin/x64/cef_extension.node'],
              frameworks: [
                'dist/darwin/x64/Chromium Embedded Framework.framework',
              ],
            },
            {
              os: 'win32',
              arch: 'x64',
              files: [
                'dist/win32/x64/cef_extension.node',
                'dist/win32/x64/libcef.dll',
              ],
            },
          ],
        },
      },
    })
  );
  fs.mkdirSync(
    path.join(
      packageRoot,
      'dist',
      'darwin',
      'x64',
      'Chromium Embedded Framework.framework'
    )
  );

  return {
    projectRoot,
    packageRoot,
    frameworksPath: frameworkPath,
    appBundlePath,
  };
}
