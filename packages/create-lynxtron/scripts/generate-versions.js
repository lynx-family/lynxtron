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
}

main().catch((e) => {
  process.stderr.write(String(e) + '\n');
  process.exit(1);
});
