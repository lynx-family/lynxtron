import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ensureDarwinFrameworkLinks } from './framework-links.js';

const packageRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const platform = process.env.npm_config_platform || process.platform;
const arch = process.env.npm_config_arch || process.arch;
const buildRoot = path.join(packageRoot, 'build');
const outputRoot = path.join(packageRoot, 'dist', platform, arch);
const cefRoot = path.join(
  packageRoot,
  '..',
  '..',
  '..',
  'third_party',
  'cef_binary'
);

await fs.rm(outputRoot, { recursive: true, force: true });
await fs.mkdir(outputRoot, { recursive: true });
const releaseRoot = path.join(buildRoot, 'Release');

await fs.copyFile(
  path.join(releaseRoot, 'cef_extension.node'),
  path.join(outputRoot, 'cef_extension.node')
);

if (platform === 'win32') {
  await fs.copyFile(
    path.join(buildRoot, 'Release', 'cef_subprocess.exe'),
    path.join(outputRoot, 'cef_subprocess.exe')
  );
  const releaseRoot = path.join(cefRoot, 'Release');
  const runtimeExtensions = new Set(['.bin', '.dll', '.json']);
  await fs.cp(releaseRoot, outputRoot, {
    recursive: true,
    force: true,
    filter: (source) =>
      source === releaseRoot || runtimeExtensions.has(path.extname(source)),
  });
  await fs.cp(path.join(cefRoot, 'Resources'), outputRoot, {
    recursive: true,
    force: true,
  });
}

if (platform === 'darwin') {
  await fs.cp(
    path.join(buildRoot, 'frameworks', 'Contents', 'Frameworks'),
    path.join(outputRoot, 'frameworks'),
    { recursive: true, force: true, verbatimSymlinks: true }
  );
  ensureDarwinFrameworkLinks(outputRoot);
}

console.log(`staged cef-webview build at ${outputRoot}`);
