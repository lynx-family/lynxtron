import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
import { spawn } from 'node:child_process';
import { createRequire } from 'node:module';
import { zip } from 'compressing';
import fetch from 'node-fetch';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);

const BASE_URL = '';

function getLynxtronVersion() {
  let pkgPath;

  try {
    pkgPath = path.join(process.cwd(), 'node_modules', '@lynx-js', 'lynxtron', 'package.json');
    const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
    return { version: pkg.version, lynxtronPkgPath: pkgPath };
  } catch (e) {
    try {
      pkgPath = path.join(__dirname, '..', '..', '..', 'node_modules', '@lynx-js', 'lynxtron', 'package.json');
      const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
      return { version: pkg.version, lynxtronPkgPath: pkgPath };
    } catch (e2) {
      throw new Error('Could not find @lynx-js/lynxtron package');
    }
  }
}

async function downloadFile(url, destPath) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to download ${url}, status: ${response.status}`);
  }
  const buffer = Buffer.from(await response.arrayBuffer());
  await fs.promises.writeFile(destPath, buffer);
}

async function extractZip(zipPath, destDir) {
  await zip.uncompress(zipPath, destDir);
}

async function downloadHeaders(version, distDir) {
  const url = `${BASE_URL}v${version}/lynxtron-v${version}-node-headers.zip`;
  const zipPath = path.join(distDir, `lynxtron-v${version}-node-headers.zip`);
  const headersPath = path.join(distDir, `v${version}`);

  if (fs.existsSync(headersPath)) {
    console.log(`Headers already exist at ${headersPath}, skipping download`);
    return headersPath;
  }

  if (!fs.existsSync(distDir)) {
    fs.mkdirSync(distDir, { recursive: true });
  }

  console.log(`Downloading headers from ${url}`);
  await downloadFile(url, zipPath);

  console.log(`Extracting headers to ${distDir}`);
  await extractZip(zipPath, distDir);

  const extractedDir = path.join(distDir, 'node_headers');
  if (fs.existsSync(extractedDir)) {
    fs.renameSync(extractedDir, headersPath);
  }

  fs.unlinkSync(zipPath);

  return headersPath;
}

// The lynxtron node-headers tarball does not ship a Windows `node.lib` import
// library. node-gyp's msvs template links native addons against
// `<nodedir>/<Configuration>/node.lib`, so on Windows we seed each expected
// configuration directory with a copy of `lynxtron.dll.lib` from
// `@lynx-js/lynxtron`, renamed to `node.lib`.
function ensureWindowsNodeLib(headersDir, lynxtronPkgPath) {
  if (process.platform !== 'win32') return;

  const lynxtronDist = path.join(path.dirname(lynxtronPkgPath), 'dist');
  const src = path.join(lynxtronDist, 'lynxtron.dll.lib');
  if (!fs.existsSync(src)) {
    console.warn(`[lynxtron-rebuild] lynxtron.dll.lib not found at ${src}; skipping node.lib seeding`);
    return;
  }

  for (const config of ['Release', 'Debug']) {
    const targetDir = path.join(headersDir, config);
    const targetLib = path.join(targetDir, 'node.lib');
    if (fs.existsSync(targetLib)) continue;
    fs.mkdirSync(targetDir, { recursive: true });
    fs.copyFileSync(src, targetLib);
    console.log(`[lynxtron-rebuild] Seeded ${targetLib} from lynxtron.dll.lib`);
  }
}

function findModulesWithNativeCode(buildPath) {
  const modules = [];
  const nodeModulesPath = path.join(buildPath, 'node_modules');

  if (!fs.existsSync(nodeModulesPath)) {
    return modules;
  }

  const packages = fs.readdirSync(nodeModulesPath, { withFileTypes: true });

  for (const pkg of packages) {
    if (pkg.name.startsWith('.')) continue;

    const pkgPath = path.join(nodeModulesPath, pkg.name);

    // Under pnpm the package entries in node_modules are symlinks into the
    // .pnpm store, so Dirent.isDirectory() (which does NOT follow symlinks)
    // reports false. Use statSync, which follows the link, to get the real
    // type. Skip anything that isn't ultimately a directory.
    let stats;
    try {
      stats = fs.statSync(pkgPath);
    } catch {
      continue; // broken symlink
    }
    if (!stats.isDirectory()) continue;

    const bindingGypPath = path.join(pkgPath, 'binding.gyp');

    if (fs.existsSync(bindingGypPath)) {
      modules.push(pkgPath);
    }
  }

  return modules;
}

function resolveNodeGypScript(modulePath) {
  const candidates = [
    path.join(modulePath, 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
    path.join(process.cwd(), 'node_modules', 'node-gyp', 'bin', 'node-gyp.js'),
  ];
  try {
    candidates.push(require.resolve('node-gyp/bin/node-gyp.js'));
  } catch {}
  for (const c of candidates) {
    if (c && fs.existsSync(c)) return c;
  }
  return null;
}

async function rebuildModule(modulePath, headersDir, electronVersion, arch) {
  return new Promise((resolve, reject) => {
    const args = [
      'rebuild',
      `--target=${electronVersion}`,
      '--runtime=electron',
      `--arch=${arch}`,
      '--build-from-source',
      '--nodedir', headersDir
    ];

    console.log(`Rebuilding ${path.basename(modulePath)}...`);

    // Prefer invoking node-gyp directly. Falling back to `npx` fails on
    // Windows because Node's `spawn` won't resolve `.cmd`/`.ps1` shims
    // without `shell: true`, and npx will also try to auto-install a fresh
    // copy of node-gyp into an unexpected path.
    const nodeGypJs = resolveNodeGypScript(modulePath);
    const command = nodeGypJs ? process.execPath : 'npx';
    const commandArgs = nodeGypJs ? [nodeGypJs, ...args] : ['node-gyp', ...args];

    // pnpm installs each package as a junction/symlink into the .pnpm store.
    // node-gyp shells out to `node -p "require('node-addon-api').gyp"` with
    // cwd at the module directory. If we pass the symlink path, Node's
    // require resolver walks up the *symlink* path and does not see the
    // package's real siblings inside the store, so `require('node-addon-api')`
    // fails. Resolve the real path so gyp runs inside the store where the
    // package's dependency junctions are visible.
    let realCwd = modulePath;
    try {
      realCwd = fs.realpathSync(modulePath);
    } catch {}

    const nodeGyp = spawn(command, commandArgs, {
      cwd: realCwd,
      stdio: 'inherit',
      shell: !nodeGypJs && process.platform === 'win32'
    });

    nodeGyp.on('error', (err) => {
      reject(err);
    });

    nodeGyp.on('close', (code) => {
      if (code === 0) {
        console.log(`Successfully rebuilt ${path.basename(modulePath)}`);
        resolve();
      } else {
        reject(new Error(`node-gyp failed to rebuild ${path.basename(modulePath)} with exit code ${code}`));
      }
    });
  });
}

export async function lynxtronRebuild(options = {}) {
  const { version, lynxtronPkgPath } = getLynxtronVersion();
  console.log(`Detected Lynxtron version: ${version}`);

  const arch = options.arch || process.arch;
  console.log(`Using architecture: ${arch}`);

  const distDir = path.join(__dirname, '..', 'dist');
  const headersDir = await downloadHeaders(version, distDir);

  console.log(`Headers downloaded to: ${headersDir}`);

  ensureWindowsNodeLib(headersDir, lynxtronPkgPath);

  const buildPath = options.buildPath || process.cwd();
  const modules = findModulesWithNativeCode(buildPath);

  if (modules.length === 0) {
    console.log('No native modules found to rebuild');
    return;
  }

  console.log(`Found ${modules.length} native module(s) to rebuild:`);
  modules.forEach(m => console.log(`  - ${path.basename(m)}`));

  for (const modulePath of modules) {
    try {
      await rebuildModule(modulePath, headersDir, version, arch);
    } catch (err) {
      console.error(`Error rebuilding ${path.basename(modulePath)}: ${err.message}`);
      throw err;
    }
  }

  console.log('Rebuild completed successfully!');
}
