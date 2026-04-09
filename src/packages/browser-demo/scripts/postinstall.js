const fs = require('fs');
const path = require('path');

async function exists(p) {
  try {
    await fs.promises.access(p);
    return true;
  } catch {
    return false;
  }
}

async function copyDir(src, dest) {
  await fs.promises.mkdir(dest, { recursive: true });
  const entries = await fs.promises.readdir(src, { withFileTypes: true });
  for (const e of entries) {
    const s = path.join(src, e.name);
    const d = path.join(dest, e.name);
    if (e.isDirectory()) {
      await copyDir(s, d);
    } else if (e.isSymbolicLink()) {
      const link = await fs.promises.readlink(s);
      try {
        await fs.promises.symlink(link, d);
      } catch {
        try {
          await fs.promises.unlink(d);
        } catch {}
        await fs.promises.symlink(link, d);
      }
    } else {
      await fs.promises.copyFile(s, d);
    }
  }
}

async function main() {
  const platform = process.platform;
  if (platform !== 'darwin') {
    console.log('skip frameworks sync: non-darwin platform');
    return;
  }
  const arch = process.arch;
  const cwd = process.cwd();
  const src = path.join(cwd, 'node_modules', '@lynx-js', 'cef-x-webview', 'dist', 'darwin', arch, 'frameworks');
  const dest = path.join(cwd, 'node_modules', '@lynx-js', 'lynxtron', 'dist', 'darwin', arch, 'lynxtron.app', 'Contents', 'Frameworks');
  const srcExists = await exists(src);
  const destExists = await exists(dest);
  if (!srcExists || !destExists) {
    console.log(`skip frameworks sync: ${srcExists ? 'dest missing' : 'src missing'}`);
    return;
  }
  await copyDir(src, dest);
  console.log('frameworks synced');
}

main().catch((err) => {
  console.error(err);
  process.exitCode = 0;
});
