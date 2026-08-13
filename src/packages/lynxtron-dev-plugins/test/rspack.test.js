// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { pathToFileURL } from 'node:url';

function createHook() {
  let callback;

  return {
    hook: {
      tap(_name, nextCallback) {
        callback = nextCallback;
      },
    },
    call(...args) {
      assert.equal(typeof callback, 'function');
      return callback(...args);
    },
    hasCallback() {
      return callback !== undefined;
    },
  };
}

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
      "import fs from 'node:fs'; fs.writeFileSync(process.env.LYNXTRON_TEST_OUTPUT, JSON.stringify(process.argv.slice(2)));\n",
    );

    if (process.platform === 'win32') {
      await fs.writeFile(
        path.join(binDir, 'lynxtron.cmd'),
        `@echo off\r\n"${process.execPath}" "${helperPath}" %*\r\n`,
      );
    } else {
      const quoteForShell = (value) => `'${value.replaceAll("'", "'\\''")}'`;
      const shimPath = path.join(binDir, 'lynxtron');
      await fs.writeFile(
        shimPath,
        `#!/bin/sh\nexec ${quoteForShell(process.execPath)} ${quoteForShell(helperPath)} "$@"\n`,
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
      '--label',
      'value with spaces',
      entry,
    ]);
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
}

test('rspack plugin resolves the npm shim and preserves spaces in arguments', async () => {
  await verifySpaceContainingArguments(
    path.resolve(import.meta.dirname, '../dist/rspack.js'),
  );
});

test('legacy plugin entry resolves the npm shim and preserves spaces in arguments', async () => {
  await verifySpaceContainingArguments(
    path.resolve(import.meta.dirname, '../index.js'),
  );
});

test('stages AutoLink native packages after production assets are emitted', async () => {
  const tempDir = await fs.mkdtemp(
    path.join(os.tmpdir(), 'lynxtron-autolink-stage-')
  );
  const outputPath = path.join(tempDir, 'dist');
  const dependencyName = '@example/native-addon';
  const dependencyRoot = path.join(
    tempDir,
    'node_modules',
    '@example',
    'native-addon'
  );
  const nativePath = path.join(
    dependencyRoot,
    'dist',
    process.platform,
    process.arch,
    'addon.node'
  );

  try {
    await fs.mkdir(path.dirname(nativePath), { recursive: true });
    await fs.writeFile(
      path.join(tempDir, 'package.json'),
      JSON.stringify({
        name: 'lynxtron-autolink-test',
        dependencies: {
          [dependencyName]: '1.0.0',
        },
      })
    );
    await fs.writeFile(
      path.join(dependencyRoot, 'package.json'),
      JSON.stringify({
        name: dependencyName,
        version: '1.0.0',
        exports: {
          './lynxtron': './lynxtron.cjs',
          './package.json': './package.json',
        },
        files: ['dist', 'lynxtron.cjs'],
      })
    );
    await fs.writeFile(
      path.join(dependencyRoot, 'lynx.lib.json'),
      JSON.stringify({
        platforms: {
          lynxtron: {
            path: 'dist',
          },
        },
      })
    );
    await fs.writeFile(
      path.join(dependencyRoot, 'lynxtron.cjs'),
      'module.exports = {};\n'
    );
    await fs.writeFile(nativePath, 'native-addon');

    const beforeRun = createHook();
    const watchRun = createHook();
    const afterEmit = createHook();
    const done = createHook();
    const compiler = {
      context: tempDir,
      options: {
        target: 'node',
        entry: {
          main: path.join(tempDir, 'main.js'),
        },
        output: {
          path: outputPath,
        },
      },
      hooks: {
        beforeRun: beforeRun.hook,
        watchRun: watchRun.hook,
        afterEmit: afterEmit.hook,
        done: done.hook,
      },
    };

    const { pluginLynxtron } = await import(
      pathToFileURL(
        path.resolve(import.meta.dirname, '../dist/rspack.js')
      ).href
    );
    pluginLynxtron().apply(compiler);

    assert.equal(afterEmit.hasCallback(), true);
    assert.equal(
      await fs
        .access(
          path.join(
            outputPath,
            '.lynxtron',
            'native',
            'node_modules',
            '@example',
            'native-addon',
            'dist',
            process.platform,
            process.arch,
            'addon.node'
          )
        )
        .then(() => true)
        .catch(() => false),
      false
    );

    await fs.rm(outputPath, { recursive: true, force: true });
    await fs.mkdir(outputPath, { recursive: true });
    await fs.writeFile(path.join(outputPath, 'main.js'), 'export {};\n');
    afterEmit.call();

    assert.equal(
      await fs.readFile(
        path.join(
          outputPath,
          '.lynxtron',
          'native',
          'node_modules',
          '@example',
          'native-addon',
          'dist',
          process.platform,
          process.arch,
          'addon.node'
        ),
        'utf8'
      ),
      'native-addon'
    );

    await fs.rm(path.join(dependencyRoot, 'dist'), {
      recursive: true,
      force: true,
    });
    assert.throws(
      () => afterEmit.call(),
      /Lynxtron AutoLink cannot stage native libraries/
    );
  } finally {
    await fs.rm(tempDir, { recursive: true, force: true });
  }
});
