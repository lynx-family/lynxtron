#!/usr/bin/env node
/**
 * postinstall script — only ensures include/ headers are accessible.
 * Does NOT copy scaffold templates (CMakeLists.txt, bindings/, module/).
 * Use `npx extension-builder scaffold` for that.
 */
import { existsSync } from 'fs';
import { dirname, join, resolve } from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const rootDir = dirname(__dirname);

const includeDir = join(rootDir, 'include');
if (existsSync(includeDir)) {
  console.log('[extension-builder] include/ headers available.');
} else {
  console.log('[extension-builder] Warning: include/ directory not found.');
  console.log('  Headers are bundled in the published package but not in local file: references.');
  console.log('  Run `node copy_include_files.cjs` in the extension-builder directory to generate them.');
}
