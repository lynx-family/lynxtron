import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const entrySource = fs.readFileSync(
  path.resolve(__dirname, '../index.cjs'),
  'utf8'
);
const cmakeSource = fs.readFileSync(
  path.resolve(__dirname, '../CMakeLists.txt'),
  'utf8'
);
const manifest = JSON.parse(
  fs.readFileSync(path.resolve(__dirname, '../lynx.lib.json'), 'utf8')
);

function loadEntry({ platform, arch, manifest, nativeResult = true }) {
  const module = { exports: {} };
  const loadedPaths = [];
  const nativeCalls = [];
  const host = { ready: true, reads: 0 };
  const context = {
    __dirname: platform === 'win32' ? 'C:\\cef-webview' : '/cef-webview',
    module,
    exports: module.exports,
    process: { platform, arch, env: { PATH: 'C:\\Windows\\System32' } },
    require(specifier) {
      if (specifier === 'path') {
        return platform === 'win32' ? path.win32 : path.posix;
      }
      if (specifier === './lynx.lib.json') {
        return manifest;
      }
      if (specifier === 'lynxtron') {
        host.reads++;
        return { app: {
          isReady: () => host.ready,
          getPath: (name) => {
            assert.equal(name, 'userData');
            return platform === 'win32' ? 'C:\\Users\\测试\\AppData\\Roaming\\Example' : '/Users/test/Library/Application Support/Example';
          },
        } };
      }
      loadedPaths.push(specifier);
      return {
        initialize(options) {
          nativeCalls.push(JSON.parse(JSON.stringify(options)));
          return nativeResult;
        },
      };
    },
  };

  vm.runInNewContext(entrySource, context, { filename: 'index.cjs' });
  return { entry: module.exports, loadedPaths, process: context.process, nativeCalls, host };
}

test('loads the Windows x64 binary selected by lynx.lib.json', () => {
  const { entry, loadedPaths, process } = loadEntry({
    platform: 'win32',
    arch: 'x64',
    manifest: {
      platforms: {
        lynxtron: {
          targets: [
            {
              os: 'darwin',
              arch: 'arm64',
              files: ['dist/darwin/arm64/cef_extension.node'],
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
    },
  });

  assert.deepEqual(loadedPaths, [
    'C:\\cef-webview\\dist\\win32\\x64\\cef_extension.node',
  ]);
  assert.equal(
    process.env.PATH,
    'C:\\cef-webview\\dist\\win32\\x64;C:\\Windows\\System32'
  );
  assert.equal(entry.initialize(), true);
});

for (const platform of ['darwin', 'win32']) {
  test(`${platform}: deferred host access, storage mapping and idempotence`, () => {
    const arch = platform === 'win32' ? 'x64' : 'arm64';
    const instance = loadEntry({ platform, arch, manifest });
    assert.equal(instance.host.reads, 0);
    instance.host.ready = false;
    assert.throws(() => instance.entry.initialize(), /after app.whenReady/);
    assert.equal(instance.nativeCalls.length, 0);
    instance.host.ready = true;
    assert.equal(instance.entry.initialize(), true);
    const pathImpl = platform === 'win32' ? path.win32 : path.posix;
    assert.equal(instance.nativeCalls[0].cachePath, '');
    assert.ok(pathImpl.isAbsolute(instance.nativeCalls[0].rootCachePath));
    assert.ok(instance.nativeCalls[0].rootCachePath.endsWith('cef-webview'));
    assert.equal(instance.entry.initialize({ persistent: false }), true);
    assert.equal(instance.nativeCalls.length, 1);
    assert.throws(() => instance.entry.initialize({ persistent: true }), /different storage/);
    const fresh = loadEntry({ platform, arch, manifest });
    const storagePath = platform === 'win32' ? 'D:\\测试\\webview' : '/tmp/test-webview';
    assert.equal(fresh.entry.initialize({ storagePath, persistent: true }), true);
    assert.deepEqual(fresh.nativeCalls, [{rootCachePath: storagePath, cachePath: pathImpl.join(storagePath, 'profile')}]);
  });
}

test('rejects invalid options and does not cache native initialization failure', () => {
  const instance = loadEntry({platform:'darwin',arch:'arm64',manifest,nativeResult:false});
  for (const options of [null, [], 1, {unknown:true}, {persistent:'true'}, {storagePath:'relative'}, {storagePath:'/tmp/a\0b'}]) {
    assert.throws(() => instance.entry.initialize(options));
  }
  assert.equal(instance.nativeCalls.length, 0);
  assert.throws(() => instance.entry.initialize(), /CEF initialization failed/);
  assert.throws(() => instance.entry.initialize(), /CEF initialization failed/);
  assert.equal(instance.nativeCalls.length, 2);
});

test('fails clearly when the installed package has no matching binary', () => {
  assert.throws(
    () =>
      loadEntry({
        platform: 'win32',
        arch: 'x64',
        manifest: {
          platforms: {
            lynxtron: {
              targets: [
                {
                  os: 'darwin',
                  arch: 'arm64',
                  files: ['dist/darwin/arm64/cef_extension.node'],
                },
              ],
            },
          },
        },
      }),
    /does not provide a binary for win32\/x64/
  );
});

test('macOS metadata publishes the package-owned CEF bundles', () => {
  const target = manifest.platforms.lynxtron.targets.find(
    ({ os, arch }) => os === 'darwin' && arch === 'arm64'
  );

  assert.deepEqual(target.frameworks, [
    'dist/darwin/arm64/frameworks/Chromium Embedded Framework.framework',
  ]);
  assert.deepEqual(target.appBundles, [
    'dist/darwin/arm64/frameworks/LynxtronWebview Helper.app',
    'dist/darwin/arm64/frameworks/LynxtronWebview Helper (Alerts).app',
    'dist/darwin/arm64/frameworks/LynxtronWebview Helper (GPU).app',
    'dist/darwin/arm64/frameworks/LynxtronWebview Helper (Plugin).app',
    'dist/darwin/arm64/frameworks/LynxtronWebview Helper (Renderer).app',
  ]);
  assert.match(
    cmakeSource,
    /set\(CEF_WEBVIEW_HELPER_NAME "LynxtronWebview"\)/
  );
  assert.match(
    cmakeSource,
    /set\(CEF_WEBVIEW_HELPER_BUNDLE_ID "org\.lynxjs\.lynxtron\.webview\.helper"\)/
  );
});

test('Windows metadata declares the CEF runtime payload', () => {
  assert.deepEqual(
    manifest.platforms.lynxtron.targets.find(
      ({ os, arch }) => os === 'win32' && arch === 'x64'
    ).files,
    [
      'dist/win32/x64/cef_extension.node',
      'dist/win32/x64/cef_subprocess.exe',
      'dist/win32/x64/chrome_100_percent.pak',
      'dist/win32/x64/chrome_200_percent.pak',
      'dist/win32/x64/chrome_elf.dll',
      'dist/win32/x64/d3dcompiler_47.dll',
      'dist/win32/x64/dxcompiler.dll',
      'dist/win32/x64/dxil.dll',
      'dist/win32/x64/icudtl.dat',
      'dist/win32/x64/libcef.dll',
      'dist/win32/x64/libEGL.dll',
      'dist/win32/x64/libGLESv2.dll',
      'dist/win32/x64/locales',
      'dist/win32/x64/resources.pak',
      'dist/win32/x64/v8_context_snapshot.bin',
      'dist/win32/x64/vk_swiftshader_icd.json',
      'dist/win32/x64/vk_swiftshader.dll',
      'dist/win32/x64/vulkan-1.dll',
    ]
  );
});
