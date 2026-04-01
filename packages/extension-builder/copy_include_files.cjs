#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

// packages/extension-builder → packages → lynxtron → lynxtron_oss_ws → lynx
const lynxRoot = process.env.LYNX_SOURCE_ROOT || path.resolve(__dirname, '../../../lynx');
// Namespace under include/lynxtron/ to avoid conflicts with consumer files
const targetDir = path.resolve(__dirname, 'include/lynxtron');

if (!fs.existsSync(targetDir)) {
  fs.mkdirSync(targetDir, { recursive: true });
  console.log(`Created target directory: ${targetDir}`);
}

/**
 * Copy file to target directory, preserving directory structure
 * @param {string} sourceFile Source file path
 * @param {string} sourceRoot Source file root directory
 * @param {string} targetRoot Target file root directory
 */
function copyFile(sourceFile, sourceRoot, targetRoot) {
  const relativePath = path.relative(sourceRoot, sourceFile);
  const targetFile = path.join(targetRoot, relativePath);
  const targetFileDir = path.dirname(targetFile);
  if (!fs.existsSync(targetFileDir)) {
    fs.mkdirSync(targetFileDir, { recursive: true });
  }
  fs.copyFileSync(sourceFile, targetFile);
  console.log(`Copied: ${sourceFile} -> ${targetFile}`);
}

/**
 * Copy all files in directory to target directory, without preserving directory structure (copy directly to target root)
 * @param {string} sourceDir Source directory path
 * @param {string} targetRoot Target file root directory
 */
function copyDirectoryFlat(sourceDir, targetRoot) {
  const files = fs.readdirSync(sourceDir);
  
  files.forEach(file => {
    const sourcePath = path.join(sourceDir, file);
    const stats = fs.statSync(sourcePath);
    
    if (stats.isDirectory()) {
      copyDirectoryFlat(sourcePath, targetRoot);
    } else {
      const fileName = path.basename(sourcePath);
      const targetFile = path.join(targetRoot, fileName);
      fs.copyFileSync(sourcePath, targetFile);
      console.log(`Copied (flat): ${sourcePath} -> ${targetFile}`);
    }
  });
}

/**
 * Copy all files in directory to target directory, preserving subdirectory structure but not the source directory itself
 * @param {string} sourceDir Source directory path
 * @param {string} targetRoot Target file root directory
 */
function copyDirectoryWithStructure(sourceDir, targetRoot) {
  const files = fs.readdirSync(sourceDir);
  
  files.forEach(file => {
    const sourcePath = path.join(sourceDir, file);
    const stats = fs.statSync(sourcePath);
    
    if (stats.isDirectory()) {
      const subDirName = path.basename(sourcePath);
      const targetSubDir = path.join(targetRoot, subDirName);
      if (!fs.existsSync(targetSubDir)) {
        fs.mkdirSync(targetSubDir, { recursive: true });
      }
      copyDirectoryWithStructure(sourcePath, targetSubDir);
    } else if (file.endsWith('.h')) {
      const fileName = path.basename(sourcePath);
      const targetFile = path.join(targetRoot, fileName);
      fs.copyFileSync(sourcePath, targetFile);
      console.log(`Copied: ${sourcePath} -> ${targetFile}`);
    }
  });
}

function resolveWeakNodeApiHeadersDir() {
  try {
    const weakNodeApiPkg = require.resolve('@lynx-js/weak-node-api/package.json', {
      paths: [__dirname],
    });
    const weakNodeApiRoot = path.dirname(weakNodeApiPkg);
    return path.join(weakNodeApiRoot, 'headers');
  } catch (error) {
    console.error('Error: Failed to resolve @lynx-js/weak-node-api from extension-builder dependencies.');
    console.error('Install dependencies before running this script.');
    process.exit(1);
  }
}

const embedderPublicDir = path.join(lynxRoot, 'platform/embedder/public');
if (fs.existsSync(embedderPublicDir)) {
  console.log('\nCopying files from lynx/platform/embedder/public...');
  copyDirectoryWithStructure(embedderPublicDir, targetDir);
} else {
  console.error(`Error: Directory not found: ${embedderPublicDir}`);
  process.exit(1);
}

const napiIncludeDir = resolveWeakNodeApiHeadersDir();
const napiTargetDir = path.join(targetDir, 'third_party/weak-node-api/headers');

if (fs.existsSync(napiIncludeDir)) {
  console.log(`\nCopying all files from ${napiIncludeDir} to ${napiTargetDir}...`);
  const files = fs.readdirSync(napiIncludeDir);
  files.forEach(file => {
    const sourceFile = path.join(napiIncludeDir, file);
    const stats = fs.statSync(sourceFile);
    if (stats.isFile()) {
      copyFile(sourceFile, napiIncludeDir, napiTargetDir);
    }
  });
} else {
  console.error(`Error: Directory not found: ${napiIncludeDir}`);
  process.exit(1);
}

const valueIncludeDir = path.join(lynxRoot, 'base/include/value');
const valueFiles = [
  'lynx_api_types.h',
  'lynx_value_api.h',
  'lynx_value_types.h'
];

if (fs.existsSync(valueIncludeDir)) {
  console.log('\nCopying files from lynx/base/include/value...');
  valueFiles.forEach(file => {
    const sourceFile = path.join(valueIncludeDir, file);
    if (fs.existsSync(sourceFile)) {
      copyFile(sourceFile, lynxRoot, targetDir);
    } else {
      console.error(`Error: File not found: ${sourceFile}`);
    }
  });
} else {
  console.error(`Error: Directory not found: ${valueIncludeDir}`);
  process.exit(1);
}

console.log('\n✅ All files copied successfully!');
