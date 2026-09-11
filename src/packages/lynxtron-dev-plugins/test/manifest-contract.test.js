import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { createRequire } from 'node:module';
import { resolveLynxtronAutoLinks } from '../dist/autolink.js';
import { applyLynxtronAutoLink } from '../dist/autolink-rspack.js';

const require = createRequire(import.meta.url);
const {
  prepareAutoLinkPackaging,
} = require('../../lynxtron-builder/autolink-packaging.js');

function fixture(t) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'lynxtron-manifest-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const pkg = path.join(root, 'node_modules', 'fixture');
  fs.mkdirSync(path.join(pkg, 'dist'), { recursive: true });
  fs.writeFileSync(
    path.join(root, 'package.json'),
    JSON.stringify({ dependencies: { fixture: '1.0.0' } })
  );
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify({
      name: 'fixture',
      exports: {
        '.': './index.cjs',
        './package.json': './package.json',
        './lynxtron': './index.cjs',
      },
    })
  );
  fs.writeFileSync(path.join(pkg, 'index.cjs'), 'module.exports = {};');
  fs.writeFileSync(path.join(pkg, 'dist/addon.node'), 'fixture');
  const manifest = {
    platforms: {
      lynxtron: {
        targets: [
          { os: 'darwin', arch: 'arm64', files: ['dist/addon.node'] },
          { os: 'win32', arch: 'x64', files: ['dist/addon.node'] },
        ],
      },
    },
  };
  return {
    root,
    pkg,
    manifest,
    write() {
      fs.writeFileSync(
        path.join(pkg, 'lynx.lib.json'),
        JSON.stringify(manifest)
      );
    },
  };
}

for (const [platform, arch] of [
  ['darwin', 'arm64'],
  ['win32', 'x64'],
]) {
  test(`fixed manifest survives resolve, stage and pack unchanged: ${platform}/${arch}`, (t) => {
    const f = fixture(t);
    f.write();
    const original = fs.readFileSync(path.join(f.pkg, 'lynx.lib.json'), 'utf8');
    const output = path.join(f.root, 'output');
    let afterEmit;
    applyLynxtronAutoLink(
      {
        context: f.root,
        options: {
          target: 'electron-main',
          entry: './main.js',
          output: { path: output },
        },
        hooks: {
          afterEmit: {
            tap(_name, fn) {
              afterEmit = fn;
            },
          },
        },
      },
      { platform, arch }
    );
    afterEmit();
    assert.equal(
      fs.readFileSync(path.join(f.pkg, 'lynx.lib.json'), 'utf8'),
      original
    );
    assert.equal(
      fs.readFileSync(
        path.join(
          output,
          '.lynxtron/native/node_modules/fixture/lynx.lib.json'
        ),
        'utf8'
      ),
      original
    );
    const result = prepareAutoLinkPackaging({
      config: { directories: { app: 'output' } },
      projectRoot: f.root,
      platform,
      arch,
    });
    assert.equal(result.libraries.length, 1);
  });
}

const invalid = [
  [
    'outside package',
    (target) => {
      target.files = ['../outside'];
    },
    /fixed package-relative/,
  ],
  [
    'glob path',
    (target) => {
      target.files = ['dist/*.node'];
    },
    /fixed package-relative/,
  ],
  [
    'platform alias',
    (target) => {
      target.os = 'macos';
    },
    /standard os\/arch/,
  ],
  [
    'Windows alias',
    (target) => {
      target.os = 'windows';
    },
    /standard os\/arch/,
  ],
  [
    'architecture alias',
    (target) => {
      target.arch = 'x86_64';
    },
    /standard os\/arch/,
  ],
  [
    'x86 alias',
    (target) => {
      target.arch = 'x86';
    },
    /standard os\/arch/,
  ],
  ...['files', 'frameworks', 'appBundles'].flatMap((field) =>
    ['platform', 'arch', 'manifestPlatform', 'manifestArch'].map((variable) => [
      `${field} variable ${variable}`,
      (target) => {
        target[field] = ['dist/${' + variable + '}/artifact'];
      },
      /fixed package-relative/,
    ])
  ),
];

for (const [name, mutate, error] of invalid) {
  test(`both consumers reject ${name}, even in an unselected target`, (t) => {
    const f = fixture(t);
    mutate(f.manifest.platforms.lynxtron.targets[1]);
    f.write();
    assert.throws(
      () =>
        resolveLynxtronAutoLinks({
          root: f.root,
          platform: 'darwin',
          arch: 'arm64',
        }),
      error
    );
    // Model a package already present in staging, bypassing the dev-plugin.
    const staged = path.join(
      f.root,
      'output/.lynxtron/native/node_modules/fixture'
    );
    fs.mkdirSync(path.dirname(staged), { recursive: true });
    fs.cpSync(f.pkg, staged, { recursive: true });
    assert.throws(
      () =>
        prepareAutoLinkPackaging({
          config: { directories: { app: 'output' } },
          projectRoot: f.root,
          platform: 'darwin',
          arch: 'arm64',
        }),
      error
    );
  });
}
