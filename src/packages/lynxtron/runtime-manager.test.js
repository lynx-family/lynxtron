import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createWriteStream, promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import yazl from 'yazl';

import {
  ensureRuntime,
  extractRuntimeArchive,
  getRuntimeDirectory,
  getRuntimeDownloadUrl,
  getRuntimeExecutablePath,
} from './runtime-manager.js';

function createZip(archivePath, entries) {
  return new Promise((resolve, reject) => {
    const zipFile = new yazl.ZipFile();
    const output = createWriteStream(archivePath);
    output.on('close', resolve);
    output.on('error', reject);
    zipFile.outputStream.on('error', reject);
    zipFile.outputStream.pipe(output);
    for (const entry of entries) {
      zipFile.addBuffer(Buffer.from(entry.content), entry.name, { mode: entry.mode });
    }
    zipFile.end();
  });
}

async function waitForFile(filePath) {
  const deadline = Date.now() + 10000;
  while (Date.now() < deadline) {
    try {
      await fs.access(filePath);
      return;
    } catch {
      await new Promise(resolve => setTimeout(resolve, 10));
    }
  }
  throw new Error(`Timed out waiting for ${filePath}`);
}

function runInstallWorker(args) {
  const workerPath = fileURLToPath(
    new URL('./test-fixtures/runtime-install-worker.mjs', import.meta.url)
  );
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [workerPath, ...args], {
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', chunk => { stdout += chunk; });
    child.stderr.on('data', chunk => { stderr += chunk; });
    child.on('error', reject);
    child.on('close', code => {
      if (code !== 0) {
        reject(new Error(`Runtime install worker exited with ${code}: ${stderr || stdout}`));
        return;
      }
      const lines = stdout.trim().split('\n');
      resolve(JSON.parse(lines.at(-1)));
    });
  });
}

test('local runtime directories isolate release and DevTool binaries', () => {
  const packageRoot = path.resolve('/tmp/example-lynxtron-package');
  assert.equal(getRuntimeDirectory('release', packageRoot), path.join(packageRoot, 'dist', 'release'));
  assert.equal(getRuntimeDirectory('devtool', packageRoot), path.join(packageRoot, 'dist', 'devtool'));
  assert.match(getRuntimeExecutablePath('devtool', packageRoot), /dist[\\/]devtool[\\/]/);
});

test('runtime download URLs use the stable release tag and variant filename', () => {
  assert.equal(
    getRuntimeDownloadUrl({ version: '2.0.0', platform: 'linux', arch: 'x64', variant: 'release' }),
    'https://github.com/lynx-family/lynxtron/releases/download/v2.0.0/lynxtron-v2.0.0-linux-x64.zip'
  );
  assert.equal(
    getRuntimeDownloadUrl({ baseUrl: 'https://downloads.example.test/', version: '2.0.0', platform: 'linux', arch: 'x64', variant: 'devtool' }),
    'https://downloads.example.test/v2.0.0/lynxtron-v2.0.0-linux-x64-devtool.zip'
  );
  assert.equal(
    getRuntimeDownloadUrl({ baseUrl: 'https://downloads.example.test', version: '2.0.0', platform: 'linux', arch: 'x64', variant: 'release' }),
    'https://downloads.example.test/v2.0.0/lynxtron-v2.0.0-linux-x64.zip'
  );
});

test('runtime installation is atomic, isolated by variant, and reused', async () => {
  const packageRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-runtime-manager-'));
  const downloads = [];
  const download = async (url, archivePath) => {
    downloads.push(url);
    await fs.writeFile(archivePath, 'synthetic archive');
  };
  const extract = async (_archivePath, { dir }) => {
    await fs.mkdir(dir, { recursive: true });
    await fs.writeFile(path.join(dir, 'lynxtron'), 'synthetic executable');
  };

  try {
    const devtool = await ensureRuntime({
      variant: 'devtool',
      customUrl: 'https://downloads.example.test/devtool.zip',
      packageRoot,
      executableRelativePath: 'lynxtron',
      platform: 'linux',
      download,
      extract,
    });
    assert.equal(devtool.downloaded, true);
    assert.equal(await fs.readFile(devtool.executablePath, 'utf8'), 'synthetic executable');

    const reused = await ensureRuntime({
      variant: 'devtool',
      customUrl: 'https://downloads.example.test/unused.zip',
      packageRoot,
      executableRelativePath: 'lynxtron',
      platform: 'linux',
      download,
      extract,
    });
    assert.equal(reused.downloaded, false);

    const release = await ensureRuntime({
      variant: 'release',
      customUrl: 'https://downloads.example.test/release.zip',
      packageRoot,
      executableRelativePath: 'lynxtron',
      platform: 'linux',
      download,
      extract,
    });
    assert.equal(release.downloaded, true);
    assert.notEqual(release.executablePath, devtool.executablePath);
    assert.deepEqual(downloads, [
      'https://downloads.example.test/devtool.zip',
      'https://downloads.example.test/release.zip',
    ]);
  } finally {
    await fs.rm(packageRoot, { recursive: true, force: true });
  }
});

test('concurrent processes install a runtime only once', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-runtime-lock-'));
  const downloadLog = path.join(root, 'downloads.log');
  const startedMarker = path.join(root, 'download-started');
  const releaseMarker = path.join(root, 'release-download');
  const workerArgs = [root, downloadLog, startedMarker, releaseMarker];

  try {
    const first = runInstallWorker([...workerArgs, 'hold']);
    await waitForFile(startedMarker);
    const second = runInstallWorker([...workerArgs, 'continue']);

    await new Promise(resolve => setTimeout(resolve, 200));
    await fs.writeFile(releaseMarker, 'release');

    const results = await Promise.all([first, second]);
    const downloads = (await fs.readFile(downloadLog, 'utf8')).trim().split('\n');
    assert.equal(downloads.length, 1);
    assert.deepEqual(
      results.map(result => result.downloaded).sort(),
      [false, true]
    );
    assert.equal(results[0].executablePath, results[1].executablePath);
    assert.equal(await fs.readFile(results[0].executablePath, 'utf8'), 'synthetic executable');
  } finally {
    await fs.rm(root, { recursive: true, force: true });
  }
});

test('runtime archives preserve executable modes and safe relative symlinks', async t => {
  if (process.platform === 'win32') {
    return t.skip('Windows runtime archives do not contain symlinks');
  }
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-runtime-archive-'));
  const archivePath = path.join(root, 'runtime.zip');
  const outputPath = path.join(root, 'output');

  try {
    await createZip(archivePath, [
      { name: 'bin/lynxtron', content: 'runtime', mode: 0o100755 },
      { name: 'bin/current', content: 'lynxtron', mode: 0o120777 },
      { name: 'lynxtron.exe', content: 'windows runtime', mode: 0o100755 },
    ]);

    await extractRuntimeArchive(archivePath, { dir: outputPath });

    assert.equal(await fs.readFile(path.join(outputPath, 'bin/current'), 'utf8'), 'runtime');
    assert.equal((await fs.lstat(path.join(outputPath, 'bin/current'))).isSymbolicLink(), true);
    assert.notEqual((await fs.stat(path.join(outputPath, 'bin/lynxtron'))).mode & 0o111, 0);
    assert.equal(await fs.readFile(path.join(outputPath, 'lynxtron.exe'), 'utf8'), 'windows runtime');
  } finally {
    await fs.rm(root, { recursive: true, force: true });
  }
});

test('runtime archives cannot overwrite files through an escaping symlink', async t => {
  if (process.platform === 'win32') {
    return t.skip('Windows runtime archives do not contain symlinks');
  }
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-runtime-escape-'));
  const archivePath = path.join(root, 'runtime.zip');
  const outputPath = path.join(root, 'output');
  const outsidePath = path.join(root, 'outside.txt');

  try {
    await fs.writeFile(outsidePath, 'original');
    await createZip(archivePath, [
      { name: 'pivot', content: '../outside.txt', mode: 0o120777 },
      { name: 'pivot', content: 'overwritten', mode: 0o100644 },
    ]);

    await assert.rejects(
      extractRuntimeArchive(archivePath, { dir: outputPath }),
      /Dangerous link path was refused/
    );
    assert.equal(await fs.readFile(outsidePath, 'utf8'), 'original');
  } finally {
    await fs.rm(root, { recursive: true, force: true });
  }
});
