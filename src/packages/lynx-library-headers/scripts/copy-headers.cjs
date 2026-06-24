#!/usr/bin/env node

const fs = require('node:fs');
const path = require('node:path');

const packageRoot = path.resolve(__dirname, '..');
const lynxRoot =
  process.env.LYNX_SOURCE_ROOT || path.resolve(packageRoot, '../../../lynx');
const includeRoot = path.join(packageRoot, 'include');

function assertDirectory(dir) {
  if (!fs.existsSync(dir) || !fs.statSync(dir).isDirectory()) {
    throw new Error(`Directory not found: ${dir}`);
  }
}

function ensureDirectory(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function copyHeaderFile(sourceFile, sourceRoot, targetRoot) {
  const relativePath = path.relative(sourceRoot, sourceFile);
  const targetFile = path.join(targetRoot, relativePath);
  ensureDirectory(path.dirname(targetFile));
  fs.copyFileSync(sourceFile, targetFile);
}

function copyHeadersWithRoot(sourceRoot, targetRoot) {
  assertDirectory(sourceRoot);

  const visit = (dir) => {
    const entries = fs.readdirSync(dir, { withFileTypes: true });

    for (const entry of entries) {
      const sourcePath = path.join(dir, entry.name);

      if (entry.isDirectory()) {
        visit(sourcePath);
      } else if (entry.isFile() && entry.name.endsWith('.h')) {
        copyHeaderFile(sourcePath, sourceRoot, targetRoot);
      }
    }
  };

  visit(sourceRoot);
}

function copyEmbedderCppWrapperHeaders(sourceRoot) {
  assertDirectory(sourceRoot);

  const publicTarget = path.join(includeRoot, 'lynx/platform/embedder/public');

  for (const entry of fs.readdirSync(sourceRoot, { withFileTypes: true })) {
    if (!entry.isFile() || !entry.name.endsWith('.h')) {
      continue;
    }

    const sourceFile = path.join(sourceRoot, entry.name);
    fs.copyFileSync(sourceFile, path.join(includeRoot, entry.name));
    ensureDirectory(publicTarget);
    fs.copyFileSync(sourceFile, path.join(publicTarget, entry.name));
  }
}

function copyWeakNodeApiHeaders(sourceRoot) {
  const weakNodeApiTarget = path.join(
    includeRoot,
    'third_party/weak-node-api/headers',
  );
  copyHeadersWithRoot(sourceRoot, weakNodeApiTarget);

  for (const entry of fs.readdirSync(sourceRoot, { withFileTypes: true })) {
    if (entry.isFile() && entry.name.endsWith('.h')) {
      fs.copyFileSync(
        path.join(sourceRoot, entry.name),
        path.join(includeRoot, entry.name),
      );
    }
  }
}

function copyLynxValueHeaders(sourceRoot) {
  const files = ['lynx_api_types.h', 'lynx_value_api.h', 'lynx_value_types.h'];

  for (const file of files) {
    const sourceFile = path.join(sourceRoot, file);
    const baseTarget = path.join(includeRoot, 'base/include/value', file);
    const lynxTarget = path.join(includeRoot, 'lynx/base/include/value', file);
    ensureDirectory(path.dirname(baseTarget));
    ensureDirectory(path.dirname(lynxTarget));
    fs.copyFileSync(sourceFile, baseTarget);
    fs.copyFileSync(sourceFile, lynxTarget);
  }
}

function writeFile(filePath, content) {
  ensureDirectory(path.dirname(filePath));
  fs.writeFileSync(filePath, content);
}

function writeWrapperHeaders() {
  writeFile(
    path.join(includeRoot, 'lynx_extension.h'),
    `#ifndef LYNX_EXTENSION_HEADERS_LYNX_EXTENSION_H_
#define LYNX_EXTENSION_HEADERS_LYNX_EXTENSION_H_

#include "capi/lynx_extension_module_capi.h"
#include "capi/lynx_extension_module_types_capi.h"
#include "capi/lynx_env_capi.h"
#include "capi/lynx_native_module_capi.h"
#include "capi/lynx_native_view_capi.h"

#endif  // LYNX_EXTENSION_HEADERS_LYNX_EXTENSION_H_
`,
  );

  writeFile(
    path.join(includeRoot, 'lynx/extension.h'),
    `#ifndef LYNX_EXTENSION_HEADERS_LYNX_EXTENSION_ALIAS_H_
#define LYNX_EXTENSION_HEADERS_LYNX_EXTENSION_ALIAS_H_

#include "../lynx_extension.h"

#endif  // LYNX_EXTENSION_HEADERS_LYNX_EXTENSION_ALIAS_H_
`,
  );
}

function writeRegistrationHeaders() {
  const registrationHeader = fs.readFileSync(
    path.join(packageRoot, 'scripts/templates/lynx-registration.h'),
    'utf8',
  );

  writeFile(path.join(includeRoot, 'lynx/registration.h'), registrationHeader);
}

fs.rmSync(includeRoot, { force: true, recursive: true });
ensureDirectory(includeRoot);

copyHeadersWithRoot(
  path.join(lynxRoot, 'platform/embedder/public/capi'),
  path.join(includeRoot, 'capi'),
);
copyHeadersWithRoot(
  path.join(lynxRoot, 'platform/embedder/public/capi'),
  path.join(includeRoot, 'lynx/platform/embedder/public/capi'),
);
copyEmbedderCppWrapperHeaders(
  path.join(lynxRoot, 'platform/embedder/public'),
);

copyLynxValueHeaders(path.join(lynxRoot, 'base/include/value'));

copyWeakNodeApiHeaders(
  path.join(lynxRoot, 'third_party/weak-node-api/headers'),
);

writeWrapperHeaders();
writeRegistrationHeaders();

console.log(`Copied Lynx library headers to ${includeRoot}`);
