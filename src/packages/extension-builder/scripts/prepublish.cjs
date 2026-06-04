#!/usr/bin/env node

/**
 * prepublishOnly script for @lynx-js/extension-builder
 *
 * 1. Runs copy_include_files.cjs to copy Lynx C API headers into include/
 * 2. Validates that all paths declared in package.json "files" actually exist
 * 3. Fails with a clear error if anything is missing
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');

// ── Step 1: Copy include files ────────────────────────────────────────────
console.log('=== Step 1: Copy include files ===');
const copyScript = path.join(ROOT, 'copy_include_files.cjs');
if (!fs.existsSync(copyScript)) {
  console.error(`ERROR: copy_include_files.cjs not found at ${copyScript}`);
  process.exit(1);
}

try {
  execSync(`node ${copyScript}`, { cwd: ROOT, stdio: 'inherit' });
} catch (e) {
  console.error('ERROR: copy_include_files.cjs failed');
  process.exit(1);
}

// ── Step 2: Validate files declared in package.json ───────────────────────
console.log('\n=== Step 2: Validate package files ===');

const pkg = JSON.parse(fs.readFileSync(path.join(ROOT, 'package.json'), 'utf-8'));
const filePatterns = pkg.files || [];
const errors = [];

for (const pattern of filePatterns) {
  // For glob patterns like "include/**/*", check that the base directory exists and is non-empty
  const basePath = pattern.split('/')[0];
  const fullBase = path.join(ROOT, basePath);

  if (pattern.includes('*')) {
    // Glob pattern — check base directory exists and has files
    if (!fs.existsSync(fullBase)) {
      errors.push(`Missing directory: ${basePath}/ (from pattern "${pattern}")`);
      continue;
    }
    if (fs.statSync(fullBase).isDirectory()) {
      const files = fs.readdirSync(fullBase);
      if (files.length === 0) {
        errors.push(`Empty directory: ${basePath}/ (from pattern "${pattern}")`);
      } else {
        console.log(`  ✓ ${pattern} → ${countFiles(fullBase)} file(s)`);
      }
    }
  } else {
    // Exact file path
    const fullPath = path.join(ROOT, pattern);
    if (!fs.existsSync(fullPath)) {
      errors.push(`Missing file: ${pattern}`);
    } else {
      console.log(`  ✓ ${pattern}`);
    }
  }
}

if (errors.length > 0) {
  console.error('\n❌ Publish validation failed:\n');
  for (const err of errors) {
    console.error(`  - ${err}`);
  }
  console.error('\nFix the issues above before publishing.');
  process.exit(1);
}

console.log('\n✅ All package files validated. Ready to publish.\n');

// ── Helpers ───────────────────────────────────────────────────────────────
function countFiles(dir) {
  let count = 0;
  for (const entry of fs.readdirSync(dir)) {
    const full = path.join(dir, entry);
    if (fs.statSync(full).isDirectory()) {
      count += countFiles(full);
    } else {
      count++;
    }
  }
  return count;
}
