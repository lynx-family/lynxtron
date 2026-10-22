const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const { createRequire } = require('node:module');
const os = require('node:os');
const path = require('node:path');

const requireToolchain = createRequire(
  require.resolve('../vendor/node_modules/app-builder-lib/package.json')
);
const electronGet = requireToolchain('@electron/get');
const {
  downloadElectronArtifactZip,
} = requireToolchain('./out/util/electronGet.js');
const runtimeArtifacts = require('../../lynxtron/runtime-artifacts.cjs');
const { prepareRuntimeConfig } = require('../runtime-config.js');

test('custom runtime filenames use distinct electron cache identities', async t => {
  const configs = [];
  t.mock.method(electronGet, 'downloadArtifact', async config => {
    configs.push(config);
    return __filename;
  });

  const common = {
    arch: 'arm64',
    version: '0.0.17',
    platformName: 'darwin',
    artifactName: 'electron',
  };
  const releaseFilename = 'lynxtron-v0.0.17-darwin-arm64.zip';
  const devtoolFilename = 'lynxtron-v0.0.17-darwin-arm64-devtool.zip';

  await downloadElectronArtifactZip({
    ...common,
    electronDownload: { customFilename: releaseFilename },
  });
  await downloadElectronArtifactZip({
    ...common,
    electronDownload: { customFilename: devtoolFilename },
  });

  assert.equal(configs.length, 2);
  assert.equal(configs[0].isGeneric, true);
  assert.equal(configs[0].artifactName, releaseFilename);
  assert.equal(configs[1].isGeneric, true);
  assert.equal(configs[1].artifactName, devtoolFilename);
  assert.notEqual(configs[0].artifactName, configs[1].artifactName);
});

test('release and DevTool downloads do not collide in either cache order', async () => {
  const releaseFilename = 'lynxtron-v0.0.17-darwin-arm64.zip';
  const devtoolFilename = 'lynxtron-v0.0.17-darwin-arm64-devtool.zip';
  const payloads = new Map([
    [`/v0.0.17/${releaseFilename}`, Buffer.from('release runtime')],
    [`/v0.0.17/${devtoolFilename}`, Buffer.from('devtool runtime')],
  ]);
  const requests = [];
  const server = http.createServer((request, response) => {
    requests.push(request.url);
    const payload = payloads.get(request.url);
    if (!payload) {
      response.writeHead(404).end();
      return;
    }
    response.writeHead(200, { 'Content-Length': payload.length });
    response.end(payload);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const { port } = server.address();
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-electron-cache-'));

  async function download(cacheRoot, customFilename) {
    const filePath = await downloadElectronArtifactZip({
      arch: 'arm64',
      version: '0.0.17',
      platformName: 'darwin',
      artifactName: 'electron',
      electronDownload: {
        cache: cacheRoot,
        customDir: 'v0.0.17',
        customFilename,
        isVerifyChecksum: false,
        mirror: `http://127.0.0.1:${port}/`,
      },
    });
    return fs.readFile(filePath, 'utf8');
  }

  try {
    for (const [index, order] of [
      [releaseFilename, devtoolFilename],
      [devtoolFilename, releaseFilename],
    ].entries()) {
      const cacheRoot = path.join(root, `order-${index}`);
      for (const filename of order) {
        assert.equal(
          await download(cacheRoot, filename),
          payloads.get(`/v0.0.17/${filename}`).toString()
        );
      }
      for (const filename of order) {
        await download(cacheRoot, filename);
      }
    }

    assert.equal(
      requests.filter(url => url.endsWith(`/${releaseFilename}`)).length,
      2
    );
    assert.equal(
      requests.filter(url => url.endsWith(`/${devtoolFilename}`)).length,
      2
    );
  } finally {
    await new Promise((resolve, reject) => {
      server.close(error => error ? reject(error) : resolve());
    });
    await fs.rm(root, { recursive: true, force: true });
  }
});

test('macOS yml architecture downloads the matching runtime archive', async () => {
  const version = '1.2.3';
  const payloads = new Map([
    [`/v${version}/lynxtron-v${version}-darwin-x64.zip`, Buffer.from('x64 runtime')],
    [`/v${version}/lynxtron-v${version}-darwin-arm64.zip`, Buffer.from('arm64 runtime')],
  ]);
  const requests = [];
  const server = http.createServer((request, response) => {
    requests.push(request.url);
    const payload = payloads.get(request.url);
    if (!payload) {
      response.writeHead(404).end();
      return;
    }
    response.writeHead(200, { 'Content-Length': payload.length });
    response.end(payload);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const { port } = server.address();
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-arch-download-'));

  try {
    for (const arch of ['x64', 'arm64']) {
      const prepared = prepareRuntimeConfig({
        config: { mac: { target: [{ target: 'dmg', arch }] } },
        args: ['--mac'],
        env: {},
        platform: 'darwin',
        defaultArch: arch === 'x64' ? 'arm64' : 'x64',
        lynxtronVersion: version,
        runtimeArtifacts,
      });
      Object.assign(prepared.config.electronDownload, {
        cache: path.join(root, arch),
        isVerifyChecksum: false,
        mirror: `http://127.0.0.1:${port}/`,
      });
      const filePath = await downloadElectronArtifactZip({
        arch,
        version,
        platformName: 'darwin',
        artifactName: 'electron',
        electronDownload: prepared.config.electronDownload,
      });
      assert.equal(await fs.readFile(filePath, 'utf8'), `${arch} runtime`);
    }

    assert.deepEqual(requests, [
      `/v${version}/lynxtron-v${version}-darwin-x64.zip`,
      `/v${version}/lynxtron-v${version}-darwin-arm64.zip`,
    ]);
  } finally {
    await new Promise((resolve, reject) => {
      server.close(error => error ? reject(error) : resolve());
    });
    await fs.rm(root, { recursive: true, force: true });
  }
});
