const childProcess = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

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
    const candidate = path.join(
      current,
      'out',
      process.env.ELECTRON_OUT_DIR || 'Debug',
      'include',
      'third_party',
      'weak-node-api',
      'headers',
      'node_api.h'
    );
    if (exists(candidate)) {
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

const prepareFallbackNodeDir = () => {
  const repoRoot = findRepoRoot();
  if (!repoRoot) {
    throw new Error('Unable to locate Lynxtron repo root for dialog-helper');
  }

  const outDir = process.env.ELECTRON_OUT_DIR || 'Debug';
  const headersDir = path.join(
    repoRoot,
    'out',
    outDir,
    'include',
    'third_party',
    'weak-node-api',
    'headers'
  );
  const configGypi = path.join(
    repoRoot,
    'out',
    outDir,
    'gen',
    'third_party',
    'node',
    'config.gypi'
  );
  const commonGypi = path.join(repoRoot, 'third_party', 'node', 'common.gypi');

  if (!exists(headersDir) || !exists(configGypi) || !exists(commonGypi)) {
    throw new Error(
      `Missing fallback node headers for dialog-helper under out/${outDir}`
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
