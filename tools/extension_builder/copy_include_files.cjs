#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

const lynxRoot = path.resolve(__dirname, '../../../../src/lynx');
const targetDir = path.resolve(__dirname, 'include');

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
    } else {
      const fileName = path.basename(sourcePath);
      const targetFile = path.join(targetRoot, fileName);
      fs.copyFileSync(sourcePath, targetFile);
      console.log(`Copied: ${sourcePath} -> ${targetFile}`);
    }
  });
}

const embedderPublicDir = path.join(lynxRoot, 'platform/embedder/public');
if (fs.existsSync(embedderPublicDir)) {
  console.log('\nCopying files from lynx/platform/embedder/public...');
  copyDirectoryWithStructure(embedderPublicDir, targetDir);
} else {
  console.error(`Error: Directory not found: ${embedderPublicDir}`);
}

const napiIncludeDir = path.join(lynxRoot, 'third_party/napi/include');
const napiFiles = [
  'js_native_api_types.h',
  'js_native_api.h',
  'primjs_napi_defines.h',
  'primjs_napi_undefs.h'
];

if (fs.existsSync(napiIncludeDir)) {
  console.log('\nCopying files from lynx/third_party/napi/include...');
  napiFiles.forEach(file => {
    const sourceFile = path.join(napiIncludeDir, file);
    if (fs.existsSync(sourceFile)) {
      copyFile(sourceFile, lynxRoot, targetDir);
    } else {
      console.error(`Error: File not found: ${sourceFile}`);
    }
  });
} else {
  console.error(`Error: Directory not found: ${napiIncludeDir}`);
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
}

console.log('\n✅ All files copied successfully!');
