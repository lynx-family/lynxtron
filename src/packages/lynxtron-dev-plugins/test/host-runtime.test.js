// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
import assert from 'node:assert/strict';
import { fork, execFileSync } from 'node:child_process';
import { once } from 'node:events';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { setTimeout as delay } from 'node:timers/promises';
import { pluginLynxtron } from '../dist/rsbuild.js';
import { createRsbuild } from '@rsbuild/core';

for (const field of ['dependencies', 'optionalDependencies']) {
  for (const module of [true, false]) {
    test(`AutoLink import shares the staged instance (${field}, ESM=${module})`, async () => {
      const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-autolink-external-'));
      const name = '@fixture/native';
      const pkg = path.join(root, 'node_modules', name);
      try {
        await fs.mkdir(pkg, { recursive: true });
        await fs.writeFile(path.join(root, 'package.json'), JSON.stringify({
          type: module ? 'module' : 'commonjs', [field]: { [name]: '*' },
        }));
        await fs.writeFile(path.join(pkg, 'package.json'), JSON.stringify({
          name, exports: { '.': './index.cjs', './lynxtron': './index.cjs' }, files: ['index.cjs', 'lynx.lib.json'],
        }));
        await fs.writeFile(path.join(pkg, 'lynx.lib.json'), JSON.stringify({
          platforms: { lynxtron: { targets: [{ os: process.platform, arch: process.arch, files: ['index.cjs'] }] } },
        }));
        await fs.writeFile(path.join(pkg, 'index.cjs'), `
          globalThis.loadedCopies = (globalThis.loadedCopies || 0) + 1;
          module.exports = { initialize() { return { count: globalThis.loadedCopies, path: __filename }; } };
        `);
        await fs.writeFile(path.join(root, 'main.js'), `
          import native from '${name}/lynxtron';
          console.log(JSON.stringify(native.initialize()));
        `);
        for (const autolink of [true, false]) {
          let modify;
          pluginLynxtron({ autolink }).setup({
            context: { rootPath: root },
            modifyRspackConfig({ handler }) { modify = handler; },
          });
          const config = { plugins: [] };
          modify(config, {
            environment: { config: { output: { target: 'node' } }, distPath: root },
            isDev: false,
          });
          for (const request of [name, `${name}/lynxtron`, `${name}/other`]) {
            const result = await new Promise((resolve, reject) => {
              config.externals[0]({ request }, (error, value) =>
                error ? reject(error) : resolve(value),
              );
            });
            assert.equal(result, autolink && request === `${name}/lynxtron`
              ? undefined : `node-commonjs ${request}`);
          }
        }
        const build = await createRsbuild({ cwd: root, rsbuildConfig: {
          source: { entry: { main: './main.js' } }, plugins: [pluginLynxtron()],
          output: { target: 'node', module, distPath: { root: 'dist' } },
        } });
        await build.build();
        const result = JSON.parse(execFileSync(process.execPath, [path.join(root, 'dist/main.js')], { encoding: 'utf8' }));
        assert.equal(result.count, 1);
        assert.equal(result.path, await fs.realpath(path.join(root, 'dist/.lynxtron/native/node_modules', name, 'index.cjs')));
      } finally {
        await fs.rm(root, { recursive: true, force: true });
      }
    });
  }
}

for (const module of [true, false]) {
  test(`built host loads a package-relative asset (ESM=${module})`, async () => {
    const root = await fs.mkdtemp(
      path.join(os.tmpdir(), 'lynxtron-host-bundle-'),
    );
    try {
      const pkg = path.join(root, 'node_modules', 'native-fixture');
      await fs.mkdir(pkg, { recursive: true });
      await fs.writeFile(
        path.join(root, 'package.json'),
        JSON.stringify({
          type: module ? 'module' : 'commonjs',
          dependencies: { 'native-fixture': '*' },
        }),
      );
      await fs.writeFile(
        path.join(pkg, 'package.json'),
        JSON.stringify({ name: 'native-fixture', main: 'index.cjs' }),
      );
      await fs.writeFile(
        path.join(pkg, 'index.cjs'),
        "module.exports = require('node:fs').readFileSync(require('node:path').join(__dirname, 'asset.txt'), 'utf8');",
      );
      await fs.writeFile(path.join(pkg, 'asset.txt'), 'package-relative-ok');
      await fs.writeFile(
        path.join(root, 'main.js'),
        "import value from 'native-fixture'; console.log(value);",
      );
      const rsbuild = await createRsbuild({
        cwd: root,
        rsbuildConfig: {
          source: { entry: { main: './main.js' } },
          plugins: [pluginLynxtron({ autolink: false })],
          output: { target: 'node', module, distPath: { root: 'dist' } },
        },
      });
      await rsbuild.build();
      assert.equal(
        execFileSync(process.execPath, [path.join(root, 'dist/main.js')], {
          encoding: 'utf8',
        }).trim(),
        'package-relative-ok',
      );
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });
}

test('host preserves production package boundaries and existing externals', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-external-'));
  try {
    await fs.writeFile(
      path.join(root, 'package.json'),
      JSON.stringify({
        dependencies: { 'better-sqlite3': '*' },
        optionalDependencies: { '@scope/native': '*' },
        devDependencies: { 'build-only': '*' },
      }),
    );
    let modify;
    pluginLynxtron().setup({
      context: { rootPath: root },
      modifyRspackConfig({ handler }) {
        modify = handler;
      },
    });
    const config = { plugins: [], externals: ['existing'] };
    modify(config, {
      environment: { config: { output: { target: 'node' } }, distPath: root },
      isDev: false,
    });
    assert.equal(config.externals[0], 'existing');
    const external = (request) =>
      new Promise((resolve, reject) => {
        config.externals[1]({ request }, (error, value) =>
          error ? reject(error) : resolve(value),
        );
      });
    for (const request of [
      'better-sqlite3',
      'better-sqlite3/lib/database',
      '@scope/native/subpath',
    ]) {
      assert.equal(await external(request), `node-commonjs ${request}`);
    }
    for (const request of [
      './better-sqlite3',
      'better-sqlite3-extra',
      '@scope/native-extra',
      'build-only',
    ]) {
      assert.equal(await external(request), undefined);
    }
  } finally {
    await fs.rm(root, { recursive: true, force: true });
  }
});

async function waitUntil(check) {
  for (let i = 0; i < 150; i++) {
    if (await check()) return;
    await delay(20);
  }
  throw new Error('Timed out waiting for process lifecycle');
}

for (const action of [
  'watchClose',
  'shutdown',
  'SIGINT',
  'SIGTERM',
  'exit',
  'cancel',
]) {
  test(
    `dev runtime cleanup: ${action}`,
    { skip: process.platform === 'win32' && action.startsWith('SIG') },
    async () => {
      const root = await fs.mkdtemp(
        path.join(os.tmpdir(), 'lynxtron-lifecycle-'),
      );
      const pidFile = path.join(root, 'pid');
      const runtime = path.join(root, 'runtime.mjs');
      const runner = path.join(root, 'runner.mjs');
      let child, pid;
      try {
        await fs.writeFile(
          runtime,
          `import fs from 'node:fs'; fs.writeFileSync(${JSON.stringify(pidFile)}, String(process.pid)); setInterval(() => {}, 1000);`,
        );
        await fs.writeFile(
          runner,
          `
        import { pluginLynxtron } from ${JSON.stringify(new URL('../dist/rspack.js', import.meta.url).href)};
        const callbacks = {};
        const hook = key => ({ tap(name, callback) { callbacks[key] = callback; } });
        pluginLynxtron({ isDev: true, autolink: false, command: process.execPath, entry: ${JSON.stringify(runtime)} }).apply({ options: {}, hooks: { done: hook('done'), watchClose: hook('watchClose'), shutdown: hook('shutdown') } });
        callbacks.done();
        if (${JSON.stringify(action)} === 'cancel') callbacks.watchClose();
        process.on('message', action => {
          if (action === 'exit') process.exit(0);
          callbacks[action]?.();
          if (action !== 'done') process.disconnect();
        });
      `,
        );
        child = fork(runner, { stdio: ['ignore', 'ignore', 'inherit', 'ipc'] });
        if (action === 'cancel') {
          await delay(600);
          await assert.rejects(fs.access(pidFile), { code: 'ENOENT' });
          child.send('shutdown');
        } else {
          await waitUntil(async () => {
            try {
              pid = Number(await fs.readFile(pidFile, 'utf8'));
              return true;
            } catch {
              return false;
            }
          });
          const exited = once(child, 'exit');
          if (action.startsWith('SIG')) child.kill(action);
          else child.send(action);
          await exited;
          await waitUntil(() => {
            try {
              process.kill(pid, 0);
              return false;
            } catch (error) {
              return error.code === 'ESRCH';
            }
          });
        }
      } finally {
        if (child && child.exitCode === null && child.signalCode === null)
          child.kill();
        if (pid) {
          try {
            process.kill(pid, 'SIGKILL');
          } catch {}
        }
        await fs.rm(root, { recursive: true, force: true });
      }
    },
  );
}
