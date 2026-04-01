#!/usr/bin/env node
/**
 * Scaffold a new Lynx native extension project.
 * Copies template files (CMakeLists.txt, bindings/, module/, index.cjs)
 * into the target directory. Skips files that already exist.
 *
 * Usage:
 *   npx extension-builder scaffold           # scaffold in current directory
 *   npx extension-builder scaffold ./my-ext   # scaffold in specific directory
 */

const fs = require('fs');
const path = require('path');

const rootDir = path.resolve(__dirname, '..');
const targetDir = process.argv[2] ? path.resolve(process.argv[2]) : process.cwd();

console.log(`[extension-builder] Scaffolding into: ${targetDir}\n`);

function copyRecursiveNoOverwrite(source, target) {
  const stat = fs.statSync(source);
  if (stat.isDirectory()) {
    if (!fs.existsSync(target)) {
      fs.mkdirSync(target, { recursive: true });
    }
    for (const item of fs.readdirSync(source)) {
      copyRecursiveNoOverwrite(path.join(source, item), path.join(target, item));
    }
  } else {
    if (fs.existsSync(target)) {
      console.log(`  ⊘ Skipped (exists): ${path.relative(targetDir, target)}`);
    } else {
      const dir = path.dirname(target);
      if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
      fs.copyFileSync(source, target);
      console.log(`  ✓ Created: ${path.relative(targetDir, target)}`);
    }
  }
}

const templates = ['CMakeLists.txt', 'index.cjs', 'bindings', 'module'];

for (const name of templates) {
  const src = path.join(rootDir, name);
  const dst = path.join(targetDir, name);
  if (!fs.existsSync(src)) {
    console.log(`  ✗ Template not found: ${name}`);
    continue;
  }
  copyRecursiveNoOverwrite(src, dst);
}

console.log('\n✅ Scaffold complete. Customize the generated files for your extension.');
