import fs from 'fs';
import fsp from 'fs/promises';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const pkgDir = path.resolve(__dirname, '..');
const srcDir = path.resolve(pkgDir, '..', 'browser-demo');
const dstDir = path.resolve(pkgDir, 'dist', 'browser-demo');

async function ensureDir(p) {
  await fsp.mkdir(p, { recursive: true });
}

async function copyDir(src, dest) {
  const entries = await fsp.readdir(src, { withFileTypes: true });
  for (const entry of entries) {
    if (entry.name === 'node_modules' || entry.name === 'dist' || entry.name === '.git') continue;
    const sp = path.join(src, entry.name);
    const dp = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      await ensureDir(dp);
      await copyDir(sp, dp);
    } else {
      await ensureDir(path.dirname(dp));
      await fsp.copyFile(sp, dp);
    }
  }
}

async function main() {
  if (!fs.existsSync(srcDir)) return;
  if (fs.existsSync(dstDir)) {
    await fsp.rm(dstDir, { recursive: true, force: true });
  }
  await ensureDir(dstDir);
  await copyDir(srcDir, dstDir);
}

main().catch((e) => {
  process.stderr.write(String(e) + '\n');
  process.exit(1);
});
