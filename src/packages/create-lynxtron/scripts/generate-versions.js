import fs from 'fs';
import fsp from 'fs/promises';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const pkgDir = path.resolve(__dirname, '..');

async function ensureDir(p) {
  await fsp.mkdir(p, { recursive: true });
}

async function readVersion(p) {
  try {
    const s = await fsp.readFile(p, 'utf8');
    return JSON.parse(s).version;
  } catch {
    return null;
  }
}

async function main() {
  const siblingRoot = path.resolve(pkgDir, '..');
  const distDir = path.resolve(pkgDir, 'dist');
  const manifestPath = path.resolve(distDir, 'versions.json');
  const manifest = {};
  try {
    const entries = await fsp.readdir(siblingRoot, { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isDirectory()) continue;
      const pkgPath = path.resolve(siblingRoot, entry.name, 'package.json');
      try {
        const s = await fsp.readFile(pkgPath, 'utf8');
        const pkg = JSON.parse(s);
        if (pkg?.name && pkg?.version) {
          manifest[pkg.name] = pkg.version;
        }
      } catch {}
    }
  } catch {}
  await ensureDir(distDir);
  await fsp.writeFile(manifestPath, JSON.stringify(manifest, null, 2) + '\n', 'utf8');
  const templateDir = path.resolve(distDir, 'lynxtron-shell-demo');
  try {
    const hasTemplate = fs.existsSync(templateDir);
    if (!hasTemplate) return;
    const replaceRange = (ver, name) => {
      if (typeof ver !== 'string') return ver;
      if (!ver.startsWith('workspace:')) return ver;
      const mapped = manifest[name];
      if (!mapped) return ver;
      const suffix = ver.slice('workspace:'.length);
      if (suffix === '^') return '^' + mapped;
      if (suffix === '~') return '~' + mapped;
      return mapped;
    };
    const replaceDeps = (deps) => {
      if (!deps) return false;
      let changed = false;
      for (const [name, ver] of Object.entries(deps)) {
        const next = replaceRange(ver, name);
        if (next !== ver) {
          deps[name] = next;
          changed = true;
        }
      }
      return changed;
    };
    const walk = async (dir) => {
      const entries = await fsp.readdir(dir, { withFileTypes: true });
      for (const entry of entries) {
        if (entry.name === 'node_modules' || entry.name === 'dist' || entry.name === '.git') continue;
        const sp = path.join(dir, entry.name);
        if (entry.isDirectory()) {
          await walk(sp);
        } else if (entry.name === 'package.json') {
          try {
            const s = await fsp.readFile(sp, 'utf8');
            const pkg = JSON.parse(s);
            const c1 = replaceDeps(pkg.dependencies);
            const c2 = replaceDeps(pkg.devDependencies);
            const c3 = replaceDeps(pkg.optionalDependencies);
            if (c1 || c2 || c3) {
              await fsp.writeFile(sp, JSON.stringify(pkg, null, 2) + '\n', 'utf8');
            }
          } catch {}
        }
      }
    };
    await walk(templateDir);
  } catch {}
}

main().catch((e) => {
  process.stderr.write(String(e) + '\n');
  process.exit(1);
});
