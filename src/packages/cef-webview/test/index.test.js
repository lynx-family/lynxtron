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
const stageBuildSource = fs.readFileSync(
  path.resolve(__dirname, '../scripts/stage-build.js'),
  'utf8'
);
const manifest = JSON.parse(
  fs.readFileSync(path.resolve(__dirname, '../lynx.lib.json'), 'utf8')
);

function loadEntry({ platform, arch, manifest }) {
  const module = { exports: {} };
  const loadedPaths = [];
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
      loadedPaths.push(specifier);
      return {
        initialize(options) {
          return { options, platform, arch };
        },
      };
    },
  };

  vm.runInNewContext(entrySource, context, { filename: 'index.cjs' });
  return { entry: module.exports, loadedPaths, process: context.process };
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
  assert.deepEqual(entry.initialize({ cachePath: 'cache' }), {
    options: { cachePath: 'cache' },
    platform: 'win32',
    arch: 'x64',
  });
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

test('Windows builds use the Lynxtron import helper and stage the CEF runtime', () => {
  assert.match(cmakeSource, /lynx_link_lynxtron_runtime/);
  assert.doesNotMatch(cmakeSource, /lxtn\.dll\.lib/);
  assert.match(stageBuildSource, /'cef_subprocess\.exe'/);
  assert.match(stageBuildSource, /runtimeExtensions/);
  assert.match(stageBuildSource, /path\.join\(cefRoot, 'Resources'\)/);
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
