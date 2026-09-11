const childProcess = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

// This addon only backs the macOS dialog end-to-end suite in
// spec/api-dialog-spec.ts, which is itself gated behind
// ELECTRON_SKIP_NATIVE_MODULE_TESTS. When that suite is skipped the compiled
// addon is never loaded, so building it here is pure overhead — and on some CI
// test runners the node-gyp compile fails and aborts the whole `yarn install`,
// which then fails specs that don't touch dialogs at all. Honor the same env
// switch here so skipping the tests also skips this build.
if (process.env.ELECTRON_SKIP_NATIVE_MODULE_TESTS) {
  console.log(
    '[dialog-helper] ELECTRON_SKIP_NATIVE_MODULE_TESTS is set; skipping native addon build.'
  );
  process.exit(0);
}

const packageRoot = path.resolve(__dirname, '..');

const exists = (targetPath) => {
  try {
    fs.accessSync(targetPath);
    return true;
  } catch {
    return false;
  }
};

const findRepoRoot = () => {
  let current = packageRoot;
  while (true) {
    if (
      exists(path.join(current, 'src', 'package.json')) &&
      exists(path.join(current, 'lynxtron_tools', 'envsetup.ps1'))
    ) {
      return current;
    }

    const parent = path.dirname(current);
    if (parent === current) {
      return null;
    }
    current = parent;
  }
};

const hasCompleteNodeDir = (nodeDir) => {
  if (!nodeDir) return false;
  return (
    exists(path.join(nodeDir, 'common.gypi')) &&
    exists(path.join(nodeDir, 'include', 'node', 'node_api.h')) &&
    exists(path.join(nodeDir, 'include', 'node', 'js_native_api.h')) &&
    exists(path.join(nodeDir, 'include', 'node', 'config.gypi'))
  );
};

const getCandidateOutDirs = () => {
  const candidates = [];
  if (process.env.LYNXTRON_OUT_DIR) {
    candidates.push(process.env.LYNXTRON_OUT_DIR);
  }

  const nodeDir = process.env.npm_config_nodedir;
  if (nodeDir) {
    const parts = path.normalize(nodeDir).split(path.sep);
    const outIndex = parts.lastIndexOf('out');
    if (outIndex >= 0 && parts[outIndex + 1]) {
      candidates.push(parts[outIndex + 1]);
    }
  }

  candidates.push('Release', 'Default', 'Debug', 'Testing');
  return [...new Set(candidates)];
};

const prepareFallbackNodeDir = () => {
  const repoRoot = findRepoRoot();
  if (!repoRoot) {
    throw new Error('Unable to locate Lynxtron repo root for dialog-helper');
  }

  let outDir = null;
  let headersDir = null;
  let configGypi = null;
  const commonGypi = path.join(repoRoot, 'third_party', 'node', 'common.gypi');

  for (const candidateOutDir of getCandidateOutDirs()) {
    const candidateHeadersDir = path.join(
      repoRoot,
      'out',
      candidateOutDir,
      'include',
      'third_party',
      'weak-node-api',
      'headers'
    );
    const candidateConfigGypi = path.join(
      repoRoot,
      'out',
      candidateOutDir,
      'gen',
      'third_party',
      'node',
      'config.gypi'
    );

    if (
      exists(candidateHeadersDir) &&
      exists(candidateConfigGypi) &&
      exists(commonGypi)
    ) {
      outDir = candidateOutDir;
      headersDir = candidateHeadersDir;
      configGypi = candidateConfigGypi;
      break;
    }
  }

  if (!outDir || !headersDir || !configGypi) {
    throw new Error(
      `Missing fallback node headers for dialog-helper under out/{${getCandidateOutDirs().join(', ')}}`
    );
  }

  const fallbackDir = path.join(packageRoot, '.node-gyp-headers');
  const includeNodeDir = path.join(fallbackDir, 'include', 'node');
  fs.rmSync(fallbackDir, { recursive: true, force: true });
  fs.mkdirSync(includeNodeDir, { recursive: true });

  for (const entry of fs.readdirSync(headersDir)) {
    fs.copyFileSync(path.join(headersDir, entry), path.join(includeNodeDir, entry));
  }
  fs.copyFileSync(configGypi, path.join(includeNodeDir, 'config.gypi'));
  fs.copyFileSync(commonGypi, path.join(fallbackDir, 'common.gypi'));

  return fallbackDir;
};

const resolvedNodeDir = hasCompleteNodeDir(process.env.npm_config_nodedir)
  ? process.env.npm_config_nodedir
  : prepareFallbackNodeDir();

const env = {
  ...process.env,
  npm_config_nodedir: resolvedNodeDir,
};

const nodeGypCli = require.resolve('node-gyp/bin/node-gyp.js');

for (const command of ['configure', 'build']) {
  const result = childProcess.spawnSync(process.execPath, [nodeGypCli, command], {
    cwd: packageRoot,
    env,
    stdio: 'inherit',
  });
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}
