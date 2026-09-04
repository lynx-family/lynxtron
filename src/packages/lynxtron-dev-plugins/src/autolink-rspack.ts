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
  // Rsbuild cleans the output directory after beforeRun. Stage native files
  // after Rspack emits assets so the cleanup cannot remove them.
  compiler.hooks?.afterEmit?.tap('LynxtronAutoLinkStage', stage);
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
  typeof __filename === 'string'
    ? __filename
    : __fileURLToPath(import.meta.url),
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

      compiler.hooks?.afterEmit?.tap('LynxtronAutoLinkStage', stage);
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
    stageAutoLinkLibrary(outputPath, library);
  }
}

function stageAutoLinkLibrary(
  outputPath: string,
  library: LynxtronAutoLinkStagedLibrary
): void {
  if (!fs.existsSync(library.sourcePath)) {
    return;
  }

  const targetPath = path.join(outputPath, library.stagedPath);

  fs.rmSync(targetPath, { recursive: true, force: true });
  fs.mkdirSync(targetPath, { recursive: true });

  for (const entry of library.files) {
    copyPath(
      path.join(library.sourcePath, entry),
      path.join(targetPath, entry)
    );
  }
}

function copyPath(sourcePath: string, targetPath: string): void {
  let stat: fs.Stats;
  try {
    stat = fs.lstatSync(sourcePath);
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === 'ENOENT') {
      return;
    }
    throw error;
  }

  if (stat.isSymbolicLink()) {
    copySymbolicLink(sourcePath, targetPath, path.dirname(sourcePath));
    return;
  }

  if (stat.isDirectory()) {
    copyDirectory(sourcePath, targetPath, sourcePath);
    return;
  }

  fs.mkdirSync(path.dirname(targetPath), { recursive: true });
  fs.copyFileSync(sourcePath, targetPath);
}

function copyDirectory(
  sourcePath: string,
  targetPath: string,
  sourceRoot = sourcePath
): void {
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
      copyDirectory(sourceEntry, targetEntry, sourceRoot);
    } else if (entry.isSymbolicLink()) {
      copySymbolicLink(sourceEntry, targetEntry, sourceRoot);
    } else if (entry.isFile()) {
      fs.mkdirSync(path.dirname(targetEntry), { recursive: true });
      fs.copyFileSync(sourceEntry, targetEntry);
    }
  }
}

function copySymbolicLink(
  sourcePath: string,
  targetPath: string,
  sourceRoot: string
): void {
  const linkTarget = fs.readlinkSync(sourcePath);
  const resolvedTarget = path.resolve(path.dirname(sourcePath), linkTarget);
  const relativeTarget = path.relative(sourceRoot, resolvedTarget);

  if (
    path.isAbsolute(linkTarget) ||
    relativeTarget === '..' ||
    relativeTarget.startsWith(`..${path.sep}`)
  ) {
    throw new Error(
      `Lynxtron AutoLink refuses to stage symbolic link outside its package: ${sourcePath} -> ${linkTarget}`
    );
  }

  fs.mkdirSync(path.dirname(targetPath), { recursive: true });
  fs.symlinkSync(linkTarget, targetPath);
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
