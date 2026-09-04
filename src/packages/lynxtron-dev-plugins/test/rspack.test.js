// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { pathToFileURL } from 'node:url';

async function waitForJson(filePath, timeoutMs = 3000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      return JSON.parse(await fs.readFile(filePath, 'utf8'));
    } catch (error) {
      if (error?.code !== 'ENOENT') throw error;
      await new Promise((resolve) => setTimeout(resolve, 25));
    }
  }
  throw new Error(`Timed out waiting for ${filePath}`);
}

async function verifySpaceContainingArguments(modulePath) {
  const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron plugin '));
  const helperPath = path.join(tempDir, 'capture arguments.mjs');
  const outputPath = path.join(tempDir, 'captured arguments.json');
  const entry = path.join(tempDir, 'app with spaces', 'dist', 'desktop');
  const binDir = path.join(tempDir, 'bin with spaces');

  try {
    await fs.mkdir(binDir, { recursive: true });
    await fs.writeFile(
      helperPath,
      "import fs from 'node:fs'; fs.writeFileSync(process.env.LYNXTRON_TEST_OUTPUT, JSON.stringify(process.argv.slice(2)));\n"
    );

    if (process.platform === 'win32') {
      await fs.writeFile(
        path.join(binDir, 'lynxtron.cmd'),
        `@echo off\r\n"${process.execPath}" "${helperPath}" %*\r\n`
      );
    } else {
      const quoteForShell = (value) => `'${value.replaceAll("'", "'\\''")}'`;
      const shimPath = path.join(binDir, 'lynxtron');
      await fs.writeFile(
        shimPath,
        `#!/bin/sh\nexec ${quoteForShell(process.execPath)} ${quoteForShell(
          helperPath
        )} "$@"\n`
      );
      await fs.chmod(shimPath, 0o755);
    }

    const { pluginLynxtron } = await import(pathToFileURL(modulePath).href);
    let doneCallback;
    const compiler = {
      options: {},
      hooks: {
        done: {
          tap(_name, callback) {
            doneCallback = callback;
          },
        },
      },
    };

    pluginLynxtron({
      isDev: true,
      entry,
      args: ['--label', 'value with spaces'],
      env: {
        LYNXTRON_TEST_OUTPUT: outputPath,
        PATH: `${binDir}${path.delimiter}${process.env.PATH ?? ''}`,
      },
      autolink: false,
    }).apply(compiler);

    assert.equal(typeof doneCallback, 'function');
    doneCallback();

    assert.deepEqual(await waitForJson(outputPath), [
      entry,
      '--label',
      'value with spaces',
    ]);
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
}

test('rspack plugin resolves the npm shim and preserves spaces in arguments', async () => {
  await verifySpaceContainingArguments(
    path.resolve(import.meta.dirname, '../dist/rspack.js')
  );
});

test('legacy plugin entry resolves the npm shim and preserves spaces in arguments', async () => {
  await verifySpaceContainingArguments(
    path.resolve(import.meta.dirname, '../index.js')
  );
});

test('AutoLink stages native packages after the output directory is cleaned', async () => {
  const tempDir = await fs.mkdtemp(
    path.join(os.tmpdir(), 'lynxtron-autolink-')
  );
  const packageName = 'fixture-native-library';
  const packageRoot = path.join(tempDir, 'node_modules', packageName);
  const outputPath = path.join(tempDir, 'dist');
  const stagedPackageRoot = path.join(
    outputPath,
    '.lynxtron',
    'native',
    'node_modules',
    packageName
  );

  try {
    await fs.mkdir(path.join(packageRoot, 'dist', 'runtime'), {
      recursive: true,
    });
    await fs.mkdir(path.join(packageRoot, 'dist', 'Fixture.framework'), {
      recursive: true,
    });
    await fs.mkdir(
      path.join(
        packageRoot,
        'dist',
        'app-bundles',
        'LynxtronWebview Helper.app',
        'Contents',
        'MacOS'
      ),
      { recursive: true }
    );
    await fs.writeFile(
      path.join(tempDir, 'package.json'),
      JSON.stringify({
        name: 'fixture-app',
        private: true,
        dependencies: { [packageName]: '1.0.0' },
      })
    );
    await fs.writeFile(
      path.join(packageRoot, 'package.json'),
      JSON.stringify({
        name: packageName,
        version: '1.0.0',
        main: 'index.cjs',
        exports: {
          '.': './index.cjs',
          './lynxtron': './index.cjs',
        },
        files: ['index.cjs', 'lynx.lib.json', 'dist/**/*'],
      })
    );
    await fs.writeFile(
      path.join(packageRoot, 'index.cjs'),
      'module.exports = {};\n'
    );
    await fs.writeFile(
      path.join(packageRoot, 'lynx.lib.json'),
      JSON.stringify({
        platforms: {
          lynxtron: {
            targets: [
              {
                os: process.platform,
                arch: process.arch,
                files: ['dist/native.node', 'dist/runtime'],
                ...(process.platform === 'darwin'
                  ? {
                      frameworks: ['dist/Fixture.framework'],
                      appBundles: [
                        'dist/app-bundles/LynxtronWebview Helper.app',
                      ],
                    }
                  : {}),
              },
            ],
          },
        },
      })
    );
    await fs.writeFile(
      path.join(packageRoot, 'dist', 'native.node'),
      'fixture'
    );
    await fs.writeFile(
      path.join(packageRoot, 'dist', 'native-win.node'),
      'fixture-win32'
    );
    await fs.writeFile(
      path.join(packageRoot, 'dist', 'runtime', 'runtime.dat'),
      'runtime'
    );
    await fs.writeFile(
      path.join(packageRoot, 'dist', 'Fixture.framework', 'framework.bin'),
      'framework'
    );
    await fs.writeFile(
      path.join(
        packageRoot,
        'dist',
        'app-bundles',
        'LynxtronWebview Helper.app',
        'Contents',
        'MacOS',
        'LynxtronWebview Helper'
      ),
      'helper'
    );
    if (process.platform === 'darwin') {
      await fs.symlink(
        'framework.bin',
        path.join(packageRoot, 'dist', 'Fixture.framework', 'framework-link')
      );
    }

    const callbacks = new Map();
    const hook = (name) => ({
      tap(_pluginName, callback) {
        callbacks.set(name, callback);
      },
    });
    const compiler = {
      context: tempDir,
      options: {
        target: 'electron-main',
        entry: { main: path.join(tempDir, 'main.js') },
        output: { path: outputPath },
        plugins: [],
      },
      hooks: {
        beforeRun: hook('beforeRun'),
        watchRun: hook('watchRun'),
        afterEmit: hook('afterEmit'),
        done: hook('done'),
      },
    };

    const { pluginLynxtron } = await import(
      pathToFileURL(path.resolve(import.meta.dirname, '../dist/rspack.js')).href
    );
    pluginLynxtron({ isDev: false }).apply(compiler);

    const generatedDir = path.join(
      tempDir,
      'node_modules',
      '.cache',
      'lynxtron-dev-plugins',
      'autolink'
    );
    const generatedFiles = await fs.readdir(generatedDir);
    for (const generatedFile of generatedFiles.filter((file) =>
      file.endsWith('.mjs')
    )) {
      assert.match(
        await fs.readFile(path.join(generatedDir, generatedFile), 'utf8'),
        /typeof __filename === 'string'/
      );
    }

    callbacks.get('beforeRun')();
    await assert.rejects(fs.access(stagedPackageRoot));

    // Rsbuild cleans the output after beforeRun and before Rspack emits files.
    await fs.rm(outputPath, { recursive: true, force: true });
    await fs.mkdir(outputPath, { recursive: true });
    await fs.writeFile(path.join(outputPath, 'main.js'), '');
    callbacks.get('afterEmit')();

    assert.equal(
      await fs.readFile(
        path.join(stagedPackageRoot, 'dist', 'runtime', 'runtime.dat'),
        'utf8'
      ),
      'runtime'
    );
    await assert.rejects(
      fs.access(path.join(stagedPackageRoot, 'dist', 'native-win.node'))
    );
    assert.equal(
      await fs.readFile(
        path.join(stagedPackageRoot, 'dist', 'native.node'),
        'utf8'
      ),
      'fixture'
    );
    if (process.platform === 'darwin') {
      assert.equal(
        await fs.readFile(
          path.join(
            stagedPackageRoot,
            'dist',
            'app-bundles',
            'LynxtronWebview Helper.app',
            'Contents',
            'MacOS',
            'LynxtronWebview Helper'
          ),
          'utf8'
        ),
        'helper'
      );
      assert.equal(
        await fs.readlink(
          path.join(
            stagedPackageRoot,
            'dist',
            'Fixture.framework',
            'framework-link'
          )
        ),
        'framework.bin'
      );
    }

    // Watch rebuilds must restore staging after another output clean as well.
    callbacks.get('watchRun')();
    await fs.rm(path.join(outputPath, '.lynxtron'), {
      recursive: true,
      force: true,
    });
    callbacks.get('afterEmit')();
    assert.equal(
      await fs.readFile(
        path.join(stagedPackageRoot, 'dist', 'native.node'),
        'utf8'
      ),
      'fixture'
    );
    if (process.platform === 'darwin') {
      assert.equal(
        await fs.readlink(
          path.join(
            stagedPackageRoot,
            'dist',
            'Fixture.framework',
            'framework-link'
          )
        ),
        'framework.bin'
      );
    }
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
});

test('AutoLink accepts only the prerelease Lynxtron artifact schema', async () => {
  const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-schema-'));
  const packageName = 'fixture-native-library';
  const packageRoot = path.join(tempDir, 'node_modules', packageName);
  const manifestPath = path.join(packageRoot, 'lynx.lib.json');

  try {
    await fs.mkdir(path.join(packageRoot, 'dist'), { recursive: true });
    await fs.writeFile(
      path.join(tempDir, 'package.json'),
      JSON.stringify({
        name: 'fixture-app',
        private: true,
        dependencies: { [packageName]: '1.0.0' },
      })
    );
    await fs.writeFile(
      path.join(packageRoot, 'package.json'),
      JSON.stringify({ name: packageName, version: '1.0.0' })
    );
    await fs.writeFile(
      path.join(packageRoot, 'dist', 'native.node'),
      'fixture'
    );
    await fs.writeFile(
      path.join(packageRoot, 'dist', 'native-win.node'),
      'fixture-win32'
    );
    await fs.mkdir(path.join(packageRoot, 'dist', 'Fixture.framework'));
    await fs.mkdir(path.join(packageRoot, 'dist', 'frameworks-win'));

    const { resolveLynxtronAutoLinks } = await import(
      pathToFileURL(path.resolve(import.meta.dirname, '../dist/autolink.js'))
        .href
    );

    await fs.writeFile(
      manifestPath,
      JSON.stringify({
        platforms: {
          lynxtron: {
            targets: [
              {
                os: 'darwin',
                arch: 'arm64',
                files: ['../outside', 'dist/native.node', 'dist/frameworks'],
                frameworks: ['dist/Fixture.framework'],
              },
            ],
          },
        },
      })
    );

    assert.deepEqual(
      resolveLynxtronAutoLinks({
        root: tempDir,
        platform: 'darwin',
        arch: 'arm64',
      }).libraries.map(
        ({ files, frameworks }) => ({ files, frameworks })
      ),
      [
        {
          files: ['dist/native.node', 'dist/frameworks'],
          frameworks: ['dist/Fixture.framework'],
        },
      ]
    );

    await fs.writeFile(
      manifestPath,
      JSON.stringify({
        platforms: {
          lynxtron: {
            targets: [
              {
                os: 'darwin',
                arch: 'arm64',
                files: ['dist/native.node'],
              },
              {
                os: 'win32',
                arch: 'x64',
                files: ['dist/native-win.node', 'dist/frameworks-win'],
              },
            ],
          },
        },
      })
    );

    assert.deepEqual(
      resolveLynxtronAutoLinks({
        root: tempDir,
        platform: 'win32',
        arch: 'x64',
      }).libraries.map(({ files, frameworks }) => ({
        files,
        frameworks,
      })),
      [
        {
          files: ['dist/native-win.node', 'dist/frameworks-win'],
          frameworks: [],
        },
      ]
    );

    await fs.writeFile(
      manifestPath,
      JSON.stringify({
        platforms: {
          lynxtron: {
            targets: [
              {
                os: process.platform,
                arch: process.arch,
                binaries: ['dist/native.node'],
              },
            ],
          },
        },
      })
    );

    assert.throws(
      () => resolveLynxtronAutoLinks({ root: tempDir }),
      /does not support binary, binaries, or resources.*use files/
    );

    await fs.writeFile(
      manifestPath,
      JSON.stringify({
        platforms: {
          lynxtron: {
            binary: {
              os: process.platform,
              arc: process.arch,
              path: 'dist/native.node',
            },
          },
        },
      })
    );

    assert.equal(
      resolveLynxtronAutoLinks({ root: tempDir }).libraries.length,
      0
    );

    await fs.writeFile(
      manifestPath,
      JSON.stringify({ platforms: { lynxtron: { path: 'dist' } } })
    );

    assert.equal(
      resolveLynxtronAutoLinks({ root: tempDir }).libraries.length,
      0
    );
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
});
