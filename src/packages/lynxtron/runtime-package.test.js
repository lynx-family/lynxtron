import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import crypto from 'node:crypto';
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import { createWriteStream } from 'node:fs';
import yazl from 'yazl';
import { ensureRuntime, ensureTargetRuntime } from './runtime-manager.js';
import { copyRuntimePackageArchive, loadRegistryOptions } from './runtime-package.js';
import artifacts from './runtime-artifacts.cjs';
const require = createRequire(import.meta.url);
const { prepare } = require('../runtime-npm/prepare.cjs');

async function zip(filename, platform) {
  const archive = new yazl.ZipFile();
  const stream = createWriteStream(filename);
  archive.outputStream.pipe(stream);
  archive.addBuffer(Buffer.from('runtime'), artifacts.getRuntimeExecutableRelativePath(platform), {mode: 0o100755});
  if (platform === 'darwin') {
    archive.addBuffer(Buffer.from('MacOS/lynxtron'), 'lynxtron.app/Contents/runtime-link', {mode: 0o120777});
  }
  archive.end();
  await new Promise((resolve, reject) => { stream.on('close', resolve); stream.on('error', reject); });
}

test('platform npm tarballs install locally and builder fetches exact target through project registry', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-registry-test-'));
  const source = path.join(root, 'archives');
  const output = path.join(root, 'packages');
  const cwd = path.join(root, 'project');
  await fs.mkdir(source); await fs.mkdir(cwd);
  const version = '1.2.3';
  const requests = [];
  let server;
  try {
    for (const [platform, arch] of artifacts.RUNTIME_PLATFORMS) {
      for (const variant of artifacts.VALID_RUNTIME_VARIANTS) {
        await zip(path.join(source, artifacts.getRuntimeArtifactFilename({platform, arch, version, variant})), platform);
      }
    }
    const packages = prepare({version, artifactsDirectory: source, outputDirectory: output});
    assert.equal(packages.length, 8);
    for (const pkg of packages) {
      const manifest = JSON.parse(await fs.readFile(path.join(output, pkg.name.split('/')[1], 'package.json'), 'utf8'));
      assert.deepEqual(manifest.os, [pkg.platform]);
      assert.deepEqual(manifest.cpu, [pkg.arch]);
      assert.equal(manifest.version, version);
      assert.equal(manifest.scripts, undefined);
    }
    const missingArchive = path.join(source, artifacts.getRuntimeArtifactFilename({platform:'linux', arch:'x64', version, variant:'release'}));
    await fs.rename(missingArchive, `${missingArchive}.saved`);
    assert.throws(() => prepare({version, artifactsDirectory:source, outputDirectory:path.join(root,'incomplete')}), /Expected exactly one/);
    await fs.rename(`${missingArchive}.saved`, missingArchive);
    const name = artifacts.getRuntimePackageName({platform:'darwin', arch:'x64', variant:'release'});
    const directory = path.join(output, name.split('/')[1]);
    const [pack] = JSON.parse(execFileSync('npm', ['pack', '--json', '--ignore-scripts'], {cwd:directory, encoding:'utf8'}));
    const tarball = await fs.readFile(path.join(directory, pack.filename));
    const integrity = `sha512-${crypto.createHash('sha512').update(tarball).digest('base64')}`;
    server = http.createServer((req, res) => {
      requests.push(req.url);
      if (req.url === '/runtime.tgz') { res.end(tarball); return; }
      const address = server.address();
      res.setHeader('content-type', 'application/json');
      res.end(JSON.stringify({name, 'dist-tags': {latest:version}, versions: {[version]: {
        name, version, dist:{tarball:`http://127.0.0.1:${address.port}/runtime.tgz`, integrity},
      }}}));
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    await fs.writeFile(path.join(cwd, '.npmrc'), `@lynx-js:registry=http://127.0.0.1:${server.address().port}/\ncache=${root}/cache\n`);
    const options = await loadRegistryOptions(cwd);
    assert.equal(options['@lynx-js:registry'], `http://127.0.0.1:${server.address().port}/`);
    const runtimeRoot = await ensureTargetRuntime({platform:'darwin', arch:'x64', version, cwd, cacheRoot:path.join(root,'runtimes')});
    assert.equal(await fs.readFile(path.join(runtimeRoot, artifacts.getRuntimeExecutableRelativePath('darwin')), 'utf8'), 'runtime');
    assert.equal(await fs.readlink(path.join(runtimeRoot, 'lynxtron.app/Contents/runtime-link')), 'MacOS/lynxtron');
    assert.ok(requests.some(url => url === '/runtime.tgz'));
    const before = requests.length;
    await ensureTargetRuntime({platform:'darwin', arch:'x64', version, cwd, cacheRoot:path.join(root,'runtimes')});
    assert.equal(requests.length, before);
    // Exercise npm's actual tarball installation without lifecycle scripts.
    const hostName = artifacts.getRuntimePackageName({platform:process.platform, arch:process.arch, variant:'devtool'});
    const hostDirectory = path.join(output, hostName.split('/')[1]);
    const [hostPack] = JSON.parse(execFileSync('npm', ['pack', '--json', '--ignore-scripts'], {cwd:hostDirectory, encoding:'utf8'}));
    await fs.writeFile(path.join(cwd, 'package.json'), JSON.stringify({name:'runtime-install-test', version:'1.0.0', private:true}));
    execFileSync('npm', ['install', '--ignore-scripts', '--no-audit', '--no-fund', path.join(hostDirectory, hostPack.filename)], {cwd, stdio:'pipe'});
    const host = await ensureRuntime({platform:process.platform, arch:process.arch, version, variant:'devtool', packageRoot:cwd});
    assert.equal(await fs.readFile(host.executablePath, 'utf8'), 'runtime');
    const packageRoot = path.join(root, 'local');
    const localName = artifacts.getRuntimePackageName({platform:'linux', arch:'x64', variant:'devtool'});
    const localPackage = path.join(packageRoot, 'node_modules', localName);
    await fs.mkdir(localPackage, {recursive:true});
    await fs.cp(path.join(output, localName.split('/')[1]), localPackage, {recursive:true});
    const local = await ensureRuntime({platform:'linux', arch:'x64', version, variant:'devtool', packageRoot, executableRelativePath:'lynxtron'});
    assert.equal(await fs.readFile(local.executablePath, 'utf8'), 'runtime');
    assert.equal((await fs.stat(local.executablePath)).mode & 0o111, 0o111);
    await assert.rejects(copyRuntimePackageArchive({platform:'linux', arch:'x64', version:'9.9.9', variant:'devtool', packageRoot, archivePath:path.join(root,'missing.zip')}), /optional dependencies enabled/);
    await assert.rejects(ensureTargetRuntime({platform:'linux', arch:'arm64', version, cwd}), /not available/);
  } finally {
    if (server) await new Promise(resolve => server.close(resolve));
    await fs.rm(root, {recursive:true, force:true});
  }
});
