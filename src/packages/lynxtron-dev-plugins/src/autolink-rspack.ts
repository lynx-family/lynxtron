// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import fs from 'node:fs';
import path from 'node:path';
import {
  createLynxtronAutoLinkStagedLibraries,
  resolveLynxtronAutoLinks,
  writeLynxtronAutoLinkModule,
} from './autolink.js';
import type {
  LynxtronAutoLinkOptions,
  LynxtronAutoLinkStagedLibrary,
} from './autolink.js';

export interface LynxtronAutoLinkPluginOptions extends LynxtronAutoLinkOptions {
  generatedDir?: string;
  entryNames?: string[];
  force?: boolean;
  nativeOutputDir?: string;
}

export function applyLynxtronAutoLink(
  compiler: any,
  options: LynxtronAutoLinkPluginOptions = {}
): void {
  if (!shouldInjectForTarget(compiler.options.target, options.force)) {
    return;
  }

  const root = path.resolve(options.root ?? compiler.context ?? process.cwd());
  const generated = ensureAutoLinkModule(root, options);
  applyAutoLinkAliases(compiler, generated.aliases);
  compiler.options.entry = prependAutoLinkEntry(
    compiler.options.entry,
    generated.modulePath,
    options.entryNames
  );

  const regenerate = () => {
    ensureAutoLinkModule(root, options);
  };
  const stage = () => {
    stageAutoLinkLibraries(root, compiler.options.output?.path, options);
  };

  compiler.hooks?.beforeRun?.tap('LynxtronAutoLink', regenerate);
  compiler.hooks?.watchRun?.tap('LynxtronAutoLink', regenerate);
  compiler.hooks?.beforeRun?.tap('LynxtronAutoLinkStage', stage);
  compiler.hooks?.watchRun?.tap('LynxtronAutoLinkStage', stage);
}

function shouldInjectForTarget(target: unknown, force = false): boolean {
  if (force) {
    return true;
  }

  if (typeof target === 'string') {
    return isNodeHostTarget(target);
  }

  if (Array.isArray(target)) {
    return target.some(
      (item) => typeof item === 'string' && isNodeHostTarget(item)
    );
  }

  return false;
}

function isNodeHostTarget(target: string): boolean {
  return (
    target === 'node' || target === 'async-node' || target === 'electron-main'
  );
}

function ensureAutoLinkModule(
  root: string,
  options: LynxtronAutoLinkPluginOptions
): {
  modulePath: string;
  stagedLibraries: LynxtronAutoLinkStagedLibrary[];
  aliases: Record<string, string>;
} {
  const generatedDir = path.resolve(
    root,
    options.generatedDir ?? 'node_modules/.cache/lynxtron-dev-plugins/autolink'
  );
  const generatedModule = path.join(generatedDir, 'register.mjs');
  const resolution = resolveLynxtronAutoLinks({
    ...options,
    root,
    warn:
      options.warn ??
      ((message) => {
        console.warn(`[lynxtron] ${message}`);
      }),
  });
  const stagedLibraries = createLynxtronAutoLinkStagedLibraries(
    resolution,
    options.nativeOutputDir
  );

  writeLynxtronAutoLinkModule(resolution, generatedModule, {
    stagedLibraries,
  });
  const aliases = writeAutoLinkProxyModules(generatedDir, stagedLibraries);
  return {
    modulePath: generatedModule,
    stagedLibraries,
    aliases,
  };
}

function applyAutoLinkAliases(
  compiler: any,
  aliases: Record<string, string>
): void {
  if (Object.keys(aliases).length === 0) {
    return;
  }

  compiler.options.resolve ??= {};

  if (
    compiler.options.resolve.alias === undefined ||
    !Array.isArray(compiler.options.resolve.alias)
  ) {
    compiler.options.resolve.alias = {
      ...(compiler.options.resolve.alias ?? {}),
      ...aliases,
    };
    return;
  }

  compiler.options.resolve.alias.push(
    ...Object.entries(aliases).map(([name, alias]) => ({ name, alias }))
  );
}

function writeAutoLinkProxyModules(
  generatedDir: string,
  stagedLibraries: LynxtronAutoLinkStagedLibrary[]
): Record<string, string> {
  const aliases: Record<string, string> = {};

  for (const library of stagedLibraries) {
    if (
      library.copyMode !== 'package' ||
      library.requireSpecifier === undefined
    ) {
      continue;
    }

    const proxyPath = path.join(
      generatedDir,
      `${sanitizeProxyFileName(library.requireSpecifier)}.mjs`
    );
    fs.mkdirSync(path.dirname(proxyPath), { recursive: true });
    fs.writeFileSync(proxyPath, generateAutoLinkProxyCode(library));
    aliases[library.requireSpecifier] = proxyPath;
  }

  return aliases;
}

function generateAutoLinkProxyCode(
  library: LynxtronAutoLinkStagedLibrary
): string {
  return `import __fs from 'node:fs';
import { createRequire as __createRequire } from 'node:module';
import __path from 'node:path';
import { fileURLToPath as __fileURLToPath } from 'node:url';

const __lynxtronAutoLinkProxySourceDir = __path.dirname(
  __fileURLToPath(import.meta.url),
);
const __lynxtronAutoLinkProxyPackageRoot = ${JSON.stringify(
    normalizeStagedPath(library.stagedPath)
  )};
const __lynxtronAutoLinkProxySpecifier = ${JSON.stringify(
    library.requireSpecifier
  )};

const __lynxtronAutoLinkProxyLoadPath = __lynxtronAutoLinkProxyResolveExistingLoadPath(
  __lynxtronAutoLinkProxyPackageRoot,
);
const __lynxtronAutoLinkProxyRequire = __createRequire(
  __path.join(__lynxtronAutoLinkProxyLoadPath, 'package.json'),
);
const __lynxtronAutoLinkProxyExports = __lynxtronAutoLinkProxyRequire(
  __lynxtronAutoLinkProxySpecifier,
);

export default __lynxtronAutoLinkProxyExports.default ?? __lynxtronAutoLinkProxyExports;
export const initialize = __lynxtronAutoLinkProxyExports.initialize;

function __lynxtronAutoLinkProxyResolveExistingLoadPath(libraryPath) {
  const attemptedPaths = [];

  for (const runtimeDir of __lynxtronAutoLinkProxyGetRuntimeDirs()) {
    const loadPath = __lynxtronAutoLinkProxyResolveNativeLoadPath(
      __path.resolve(runtimeDir, libraryPath),
    );
    attemptedPaths.push(loadPath);

    if (__fs.existsSync(loadPath)) {
      return loadPath;
    }
  }

  throw new Error(
    \`Cannot find Lynxtron AutoLink native library \${libraryPath}. Tried:\\n\${attemptedPaths.join('\\n')}\`,
  );
}

function __lynxtronAutoLinkProxyGetRuntimeDirs() {
  const runtimeDirs = [];
  const pushDir = (dir) => {
    if (typeof dir === 'string' && dir.length > 0) {
      runtimeDirs.push(__path.resolve(dir));
    }
  };

  if (Array.isArray(process.argv) && process.argv.length > 1) {
    pushDir(__lynxtronAutoLinkProxyResolveAppArg(process.argv[1]));
  }

  if (
    typeof process.resourcesPath === 'string' &&
    process.resourcesPath.length > 0
  ) {
    pushDir(__path.join(process.resourcesPath, 'app.asar'));
    pushDir(__path.join(process.resourcesPath, 'app'));
    pushDir(process.resourcesPath);
  }

  pushDir(__lynxtronAutoLinkProxySourceDir);
  pushDir(process.cwd());

  return Array.from(new Set(runtimeDirs));
}

function __lynxtronAutoLinkProxyResolveAppArg(value) {
  const appPath = __path.resolve(process.cwd(), value);

  try {
    return __fs.statSync(appPath).isDirectory()
      ? appPath
      : __path.dirname(appPath);
  } catch {
    return appPath;
  }
}

function __lynxtronAutoLinkProxyResolveNativeLoadPath(absolutePath) {
  const asarSegment = \`\${__path.sep}app.asar\${__path.sep}\`;
  const asarIndex = absolutePath.indexOf(asarSegment);

  if (asarIndex < 0) {
    return absolutePath;
  }

  const unpackedPath = [
    absolutePath.slice(0, asarIndex),
    \`\${__path.sep}app.asar.unpacked\${__path.sep}\`,
    absolutePath.slice(asarIndex + asarSegment.length),
  ].join('');
  return __fs.existsSync(unpackedPath) ? unpackedPath : absolutePath;
}
`;
}

function sanitizeProxyFileName(value: string): string {
  return value.replaceAll(/[^a-zA-Z0-9._-]/g, '_');
}

function normalizeStagedPath(value: string): string {
  return value.replaceAll('\\', '/').replace(/^\.\//, '');
}

function createLynxtronAutoLinkStagePlugin(
  root: string,
  options: LynxtronAutoLinkPluginOptions
): { name: string; apply: (compiler: any) => void } {
  return {
    name: 'LynxtronAutoLinkStage',
    apply(compiler: any) {
      const stage = () => {
        stageAutoLinkLibraries(root, compiler.options.output?.path, options);
      };

      compiler.hooks?.beforeRun?.tap('LynxtronAutoLinkStage', stage);
      compiler.hooks?.watchRun?.tap('LynxtronAutoLinkStage', stage);
    },
  };
}

function stageAutoLinkLibraries(
  root: string,
  outputPath: unknown,
  options: LynxtronAutoLinkPluginOptions
): void {
  if (typeof outputPath !== 'string' || outputPath.length === 0) {
    options.warn?.(
      'Lynxtron AutoLink cannot stage native libraries without an output path.'
    );
    return;
  }

  const resolution = resolveLynxtronAutoLinks({
    ...options,
    root,
    warn:
      options.warn ??
      ((message) => {
        console.warn(`[lynxtron] ${message}`);
      }),
  });
  const stagedLibraries = createLynxtronAutoLinkStagedLibraries(
    resolution,
    options.nativeOutputDir
  );

  for (const library of stagedLibraries) {
    stageAutoLinkLibrary(outputPath, library, options);
  }
}

function stageAutoLinkLibrary(
  outputPath: string,
  library: LynxtronAutoLinkStagedLibrary,
  options: LynxtronAutoLinkPluginOptions
): void {
  if (library.copyMode === 'package') {
    stageAutoLinkPackage(outputPath, library, options);
    return;
  }

  if (hasGlob(library.sourcePath)) {
    options.warn?.(
      `Lynxtron AutoLink cannot stage glob native library path: ${library.sourcePath}`
    );
    return;
  }

  if (!fs.existsSync(library.sourcePath)) {
    return;
  }

  const targetPath = path.join(outputPath, library.stagedPath);
  fs.mkdirSync(path.dirname(targetPath), { recursive: true });
  fs.copyFileSync(library.sourcePath, targetPath);
}

function stageAutoLinkPackage(
  outputPath: string,
  library: LynxtronAutoLinkStagedLibrary,
  options: LynxtronAutoLinkPluginOptions
): void {
  if (!fs.existsSync(library.sourcePath)) {
    return;
  }

  const targetPath = path.join(outputPath, library.stagedPath);
  const packageFiles = readPackageFiles(library.sourcePath);

  fs.rmSync(targetPath, { recursive: true, force: true });
  fs.mkdirSync(targetPath, { recursive: true });
  copyPath(
    path.join(library.sourcePath, 'package.json'),
    path.join(targetPath, 'package.json')
  );
  copyPath(
    path.join(library.sourcePath, 'lynx.lib.json'),
    path.join(targetPath, 'lynx.lib.json')
  );

  if (packageFiles.length === 0) {
    copyDirectory(library.sourcePath, targetPath);
    return;
  }

  for (const entry of packageFiles) {
    const normalizedEntry = normalizePackageFileEntry(entry);

    if (normalizedEntry === undefined) {
      options.warn?.(
        `Lynxtron AutoLink cannot stage package file pattern: ${entry}`
      );
      continue;
    }

    copyPath(
      path.join(library.sourcePath, normalizedEntry),
      path.join(targetPath, normalizedEntry)
    );
  }
}

function readPackageFiles(packageRoot: string): string[] {
  try {
    const packageJson = JSON.parse(
      fs.readFileSync(path.join(packageRoot, 'package.json'), 'utf8')
    ) as { files?: unknown };

    if (Array.isArray(packageJson.files)) {
      return packageJson.files.filter(
        (item): item is string =>
          typeof item === 'string' && item.length > 0 && !item.startsWith('!')
      );
    }
  } catch {}

  return [];
}

function normalizePackageFileEntry(entry: string): string | undefined {
  const normalizedEntry = entry
    .replaceAll('\\', '/')
    .replace(/^\.\//, '')
    .replace(/\/\*\*\/\*$/, '')
    .replace(/\/\*$/, '');

  if (
    normalizedEntry.length === 0 ||
    normalizedEntry.includes('..') ||
    normalizedEntry.includes('*')
  ) {
    return undefined;
  }

  return normalizedEntry;
}

function copyPath(sourcePath: string, targetPath: string): void {
  if (!fs.existsSync(sourcePath)) {
    return;
  }

  const stat = fs.statSync(sourcePath);

  if (stat.isDirectory()) {
    copyDirectory(sourcePath, targetPath);
    return;
  }

  fs.mkdirSync(path.dirname(targetPath), { recursive: true });
  fs.copyFileSync(sourcePath, targetPath);
}

function copyDirectory(sourcePath: string, targetPath: string): void {
  if (shouldSkipPackageEntry(path.basename(sourcePath))) {
    return;
  }

  fs.mkdirSync(targetPath, { recursive: true });

  for (const entry of fs.readdirSync(sourcePath, { withFileTypes: true })) {
    if (shouldSkipPackageEntry(entry.name)) {
      continue;
    }

    const sourceEntry = path.join(sourcePath, entry.name);
    const targetEntry = path.join(targetPath, entry.name);

    if (entry.isDirectory()) {
      copyDirectory(sourceEntry, targetEntry);
    } else if (entry.isFile()) {
      fs.mkdirSync(path.dirname(targetEntry), { recursive: true });
      fs.copyFileSync(sourceEntry, targetEntry);
    }
  }
}

function shouldSkipPackageEntry(name: string): boolean {
  return (
    name === 'node_modules' ||
    name === 'build' ||
    name === '.git' ||
    name === '.DS_Store' ||
    name === '_tmp_extract' ||
    name.endsWith('.zip')
  );
}

function hasGlob(value: string): boolean {
  return value.includes('*');
}

function prependAutoLinkEntry(
  entry: unknown,
  generatedModule: string,
  entryNames = ['main']
): unknown {
  if (entry === undefined) {
    return [generatedModule];
  }

  if (typeof entry === 'string') {
    return prependImport(entry, generatedModule);
  }

  if (Array.isArray(entry)) {
    return prependImport(entry, generatedModule);
  }

  if (typeof entry === 'function') {
    return async (...args: unknown[]) => {
      const resolvedEntry = await entry(...args);
      return prependAutoLinkEntry(resolvedEntry, generatedModule, entryNames);
    };
  }

  if (entry !== null && typeof entry === 'object') {
    const nextEntry: Record<string, unknown> = {
      ...(entry as Record<string, unknown>),
    };
    const names = new Set(entryNames);

    for (const name of Object.keys(nextEntry)) {
      if (names.size > 0 && !names.has(name)) {
        continue;
      }

      nextEntry[name] = prependEntryValue(nextEntry[name], generatedModule);
    }

    return nextEntry;
  }

  return entry;
}

function prependEntryValue(value: unknown, generatedModule: string): unknown {
  if (typeof value === 'string' || Array.isArray(value)) {
    return prependImport(value, generatedModule);
  }

  if (value !== null && typeof value === 'object') {
    const descriptor = { ...(value as Record<string, unknown>) };
    const currentImport = descriptor.import;

    if (typeof currentImport === 'string' || Array.isArray(currentImport)) {
      descriptor.import = prependImport(currentImport, generatedModule);
    }

    return descriptor;
  }

  return value;
}

function prependImport(
  value: string | unknown[],
  generatedModule: string
): unknown[] {
  const imports = Array.isArray(value) ? value : [value];

  if (imports.includes(generatedModule)) {
    return imports;
  }

  if (!fs.existsSync(generatedModule)) {
    throw new Error(
      `Lynxtron AutoLink generated module does not exist: ${generatedModule}`
    );
  }

  return [generatedModule, ...imports];
}
