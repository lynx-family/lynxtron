import assert from 'node:assert/strict';
import fs from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';
import path from 'node:path';
import os from 'node:os';
import { spawnSync } from 'node:child_process';

const source = fs
  .readFileSync(new URL('../scripts/build.js', import.meta.url), 'utf8')
  .replace(/^import .*;\n/gm, '')
  .replace('const require = createRequire(import.meta.url);', '');

test('CEF receives the Node target before choosing compiler architectures', () => {
  const cmake = fs.readFileSync(
    new URL('../CMakeLists.txt', import.meta.url),
    'utf8'
  );
  const setup = cmake.slice(0, cmake.indexOf('find_package(CEF REQUIRED)'));
  assert.match(
    setup,
    /if\(NODE_ARCH STREQUAL "x64"\)\s+set\(PROJECT_ARCH "x86_64"\)/
  );
  assert.match(
    setup,
    /elseif\(NODE_ARCH STREQUAL "arm64"\)\s+set\(PROJECT_ARCH "arm64"\)/
  );
});

for (const arch of ['x64', 'arm64']) {
  test(`arm64 Node passes explicit ${arch} target to cmake-js`, () => {
    let invocation;
    let exitCode;
    const require = { resolve: () => '/cmake-js/bin/cmake-js' };
    vm.runInNewContext(source, {
      require,
      process: {
        arch: 'arm64',
        execPath: '/host/node',
        env: { npm_config_arch: arch },
        exit: (code) => {
          exitCode = code;
        },
      },
      spawnSync: (...args) => {
        invocation = args;
        return { status: 0 };
      },
    });
    assert.equal(invocation[0], '/host/node');
    assert.deepEqual(Array.from(invocation[1]), [
      '/cmake-js/bin/cmake-js',
      'rebuild',
      '--arch',
      arch,
    ]);
    assert.equal(exitCode, 0);
  });
}

function runWindowsBuild(env, isFile = true) {
  let invocation;
  vm.runInNewContext(source, {
    require: { resolve: () => 'C:/tools/cmake-js/bin/cmake-js' },
    path: path.win32,
    fs: { statSync: () => ({ isFile: () => isFile }) },
    process: {
      arch: 'x64',
      execPath: 'C:/tools/node.exe',
      env,
      exit() {},
    },
    spawnSync: (...args) => {
      invocation = args;
      return { status: 0 };
    },
  });
  return invocation;
}

test('Windows source import library is forwarded as one CMake argument, including spaces', () => {
  const library = 'C:\\build directory\\out\\Release\\lynxtron.dll.lib';
  const invocation = runWindowsBuild({ LYNXTRON_IMPORT_LIB: library });
  assert.equal(invocation[0], 'C:/tools/node.exe');
  assert.equal(invocation[1].at(-1), `--CDLYNXTRON_IMPORT_LIB=${library}`);
  assert.equal(invocation[2].shell, undefined);
});

test('Windows npm builds leave the existing import-library resolver in control', () => {
  assert.equal(
    runWindowsBuild({})[1].some((arg) => arg.startsWith('--CD')),
    false
  );
});

test('invalid source import library fails without falling back to npm', () => {
  for (const library of ['', 'out/Release/lynxtron.dll.lib']) {
    assert.throws(
      () => runWindowsBuild({ LYNXTRON_IMPORT_LIB: library }),
      /existing absolute file/
    );
  }
  assert.throws(
    () =>
      runWindowsBuild(
        { LYNXTRON_IMPORT_LIB: 'C:/missing/lynxtron.dll.lib' },
        false
      ),
    /existing absolute file/
  );
});

test('CMake resolves the explicit source library without consulting npm', (t) => {
  const probe = spawnSync('cmake', ['--version']);
  if (probe.error?.code === 'ENOENT') return t.skip('CMake is not installed');
  assert.equal(probe.status, 0);
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'cef import library '));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const library = path.join(root, 'lynxtron.dll.lib').replaceAll('\\', '/');
  const helper = path
    .resolve(
      import.meta.dirname,
      '../../lynx-library-headers/cmake/lynx-library-headers.cmake'
    )
    .replaceAll('\\', '/');
  const script = path.join(root, 'resolve.cmake');
  fs.writeFileSync(library, 'fixture import library');
  fs.writeFileSync(
    script,
    `
set(WIN32 TRUE)
set(NODE_EXECUTABLE "must-not-run-node")
include("${helper}")
lynx_resolve_lynxtron_import_library(resolved)
if(NOT resolved STREQUAL LYNXTRON_IMPORT_LIB)
  message(FATAL_ERROR "Wrong import library")
endif()
`
  );
  const invoke = () =>
    spawnSync('cmake', [`-DLYNXTRON_IMPORT_LIB=${library}`, '-P', script], {
      encoding: 'utf8',
    });
  const result = invoke();
  assert.equal(result.status, 0, result.stderr);
  fs.unlinkSync(library);
  const missing = invoke();
  assert.notEqual(missing.status, 0);
  assert.match(missing.stderr, /LYNXTRON_IMPORT_LIB does not exist/);
});
