#!/usr/bin/env node
import { existsSync, copyFileSync, mkdirSync, readdirSync, statSync } from 'fs';
import { dirname, join, resolve } from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const rootDir = dirname(__dirname);

// Recursive copy function
function copyRecursiveSync(source, target) {
  const sourceStat = statSync(source);
  
  if (sourceStat.isDirectory()) {
    // Create target directory if it doesn't exist
    if (!existsSync(target)) {
      mkdirSync(target, { recursive: true });
    }
    
    // Copy all items in the directory
    const items = readdirSync(source);
    for (const item of items) {
      const sourcePath = join(source, item);
      const targetPath = join(target, item);
      copyRecursiveSync(sourcePath, targetPath);
    }
  } else {
    copyFileSync(source, target);
  }
}

// Determine installation directory based on whether it's a local dependency or not
// For local dependencies (file:extension_builder), the script path won't include /node_modules/
let installationDir;

// Check if we're in a local dependency situation (not installed via npm into node_modules)
const isLocalDependency = !__filename.includes('/node_modules/');

if (isLocalDependency) {
  // We're running as a local dependency, go up one level to reach the tools directory
  installationDir = resolve(__dirname, '..', '..', '..');
} else {
  // Normal npm installation, go up three levels: scripts -> extension-builder -> scope -> node_modules -> project root
  installationDir = resolve(__dirname, '..', '..', '..', '..');
}
  
// Copy requested files and directories to installation directory
// 1. Copy CMakeLists.txt
const cmakeListsPath = join(rootDir, 'CMakeLists.txt');
const targetCmakeListsPath = join(installationDir, 'CMakeLists.txt');
if (existsSync(cmakeListsPath)) {
  copyFileSync(cmakeListsPath, targetCmakeListsPath);
} else {
  console.log('❌ CMakeLists.txt not found in source directory');
}

// 2. Copy index.cjs
const indexCjsPath = join(rootDir, 'index.cjs');
const targetIndexCjsPath = join(installationDir, 'index.cjs');
if (existsSync(indexCjsPath)) {
  copyFileSync(indexCjsPath, targetIndexCjsPath);
} else {
  console.log('❌ index.cjs not found in source directory');
}

// 3. Copy bindings directory
const bindingsPath = join(rootDir, 'bindings');
const targetBindingsPath = join(installationDir, 'bindings');
if (existsSync(bindingsPath)) {
  copyRecursiveSync(bindingsPath, targetBindingsPath);
} else {
  console.log('❌ bindings directory not found in source directory');
}

// 4. Copy module directory
const modulePath = join(rootDir, 'module');
const targetModulePath = join(installationDir, 'module');
if (existsSync(modulePath)) {
  copyRecursiveSync(modulePath, targetModulePath);
} else {
  console.log('❌ module directory not found in source directory');
}
console.log('\n✅ All requested files and directories have been copied successfully!');
