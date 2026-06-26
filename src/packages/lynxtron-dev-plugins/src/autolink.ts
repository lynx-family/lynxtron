// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';

const DEFAULT_DEPENDENCY_FIELDS = [
  'dependencies',
  'devDependencies',
  'optionalDependencies',
] as const;
export const DEFAULT_LYNXTRON_NATIVE_OUTPUT_DIR = '.lynxtron/native';

const PLATFORM_ALIASES: Record<string, string[]> = {
  darwin: ['macos', 'darwin'],
  macos: ['macos', 'darwin'],
  win32: ['windows', 'win32'],
  windows: ['windows', 'win32'],
  linux: ['linux'],
};

const ARCH_ALIASES: Record<string, string[]> = {
  arm64: ['arm64'],
  x64: ['x64', 'x86_64'],
  x86_64: ['x64', 'x86_64'],
  ia32: ['ia32', 'x86'],
  x86: ['ia32', 'x86'],
};

export interface LynxtronAutoLinkOptions {
  root?: string;
  platform?: NodeJS.Platform | string;
  arch?: NodeJS.Architecture | string;
  dependencyFields?: string[];
  warn?: (message: string) => void;
}

export interface LynxDesktopManifestEntry {
  src?: string;
  binary?: LynxNodeApiBinaryEntry | LynxNodeApiBinaryEntry[];
}

export interface LynxNodeApiManifestEntry {
  path?: string | string[];
  binary?: LynxNodeApiBinaryEntry | LynxNodeApiBinaryEntry[];
}

export interface LynxNodeApiBinaryEntry {
  os?: string;
  arch?: string;
  arc?: string;
  path?: string | string[];
}

export type LynxtronAutoLinkStageMode = 'file' | 'package';

export interface LynxtronAutoLinkLibrary {
  name: string;
  packageRoot: string;
  packageJsonPath: string;
  manifestPath: string;
  manifest: Record<string, unknown>;
  platformKey: string;
  archKey: string;
  libs: string[];
  libPaths: string[];
  stageMode: LynxtronAutoLinkStageMode;
  nodeModulesPath: string;
  warnings: string[];
}

export interface LynxtronAutoLinkResolution {
  root: string;
  platform: string;
  arch: string;
  libraries: LynxtronAutoLinkLibrary[];
  warnings: string[];
}

export interface LynxtronAutoLinkFileSet {
  from: string;
  to: string;
  filter: string[];
}

export interface LynxtronAutoLinkStagedLibrary {
  name: string;
  sourcePath: string;
  stagedPath: string;
  requirePath: string;
  requireSpecifier?: string;
  copyMode: LynxtronAutoLinkStageMode;
}

export interface LynxtronAutoLinkCodegenOptions {
  stagedLibraries?: LynxtronAutoLinkStagedLibrary[];
}

interface PackageJson {
  name?: string;
  dependencies?: Record<string, string>;
  devDependencies?: Record<string, string>;
  optionalDependencies?: Record<string, string>;
}

interface MatchedManifestEntry {
  platformKey: string;
  archKey: string;
  libs: string[];
  stageMode: LynxtronAutoLinkStageMode;
}

export function resolveLynxtronAutoLinks(
  options: LynxtronAutoLinkOptions = {}
): LynxtronAutoLinkResolution {
  const root = path.resolve(options.root ?? process.cwd());
  const platform = String(options.platform ?? process.platform);
  const arch = String(options.arch ?? process.arch);
  const dependencyFields = options.dependencyFields ?? [
    ...DEFAULT_DEPENDENCY_FIELDS,
  ];
  const warnings: string[] = [];
  const packageJsonPath = path.join(root, 'package.json');
  const packageJson = readJson<PackageJson>(packageJsonPath);

  if (packageJson === undefined) {
    throw new Error(`Cannot read package.json from ${packageJsonPath}`);
  }

  const dependencyNames = collectDependencyNames(packageJson, dependencyFields);
  const libraries: LynxtronAutoLinkLibrary[] = [];

  for (const dependencyName of dependencyNames) {
    const resolvedPackage = resolvePackageRoot(root, dependencyName);

    if (resolvedPackage === undefined) {
      continue;
    }

    const manifestPath = path.join(
      resolvedPackage.packageRoot,
      'lynx.lib.json'
    );
    const manifest = readJson<Record<string, unknown>>(manifestPath);

    if (manifest === undefined) {
      continue;
    }

    const matchedEntry = getNodeApiManifestEntry(manifest, platform, arch);

    if (matchedEntry === undefined) {
      continue;
    }

    const libs = matchedEntry.libs.map((libraryPath) =>
      expandManifestVariables(
        libraryPath,
        platform,
        arch,
        matchedEntry.platformKey,
        matchedEntry.archKey
      )
    );

    if (libs.length === 0) {
      continue;
    }

    const libPaths = libs.map((libraryPath) =>
      path.join(resolvedPackage.packageRoot, libraryPath)
    );
    const libraryWarnings: string[] = [];

    if (matchedEntry.stageMode === 'package') {
      const requireFromPackage = createRequire(resolvedPackage.packageJsonPath);
      const lynxtronSpecifier = getNodeApiPackageSpecifier(dependencyName);

      try {
        requireFromPackage.resolve(lynxtronSpecifier);
      } catch {
        libraryWarnings.push(
          `Lynxtron AutoLink package "${dependencyName}" declares Lynxtron assets, but ${lynxtronSpecifier} cannot be resolved.`
        );
      }

      for (const [index, libPath] of libPaths.entries()) {
        if (hasGlob(libs[index])) {
          continue;
        }

        if (!fs.existsSync(libPath)) {
          libraryWarnings.push(
            `Lynxtron AutoLink package "${dependencyName}" declares Lynxtron assets ${libs[index]}, but the path does not exist.`
          );
        }
      }
    } else {
      for (const [index, libPath] of libPaths.entries()) {
        if (hasGlob(libs[index])) {
          continue;
        }

        if (!fs.existsSync(libPath)) {
          libraryWarnings.push(
            `Lynxtron AutoLink package "${dependencyName}" declares Node-API binary ${libs[index]}, but the file does not exist.`
          );
        }
      }
    }

    for (const warning of libraryWarnings) {
      warnings.push(warning);
      options.warn?.(warning);
    }

    libraries.push({
      name: dependencyName,
      packageRoot: resolvedPackage.packageRoot,
      packageJsonPath: resolvedPackage.packageJsonPath,
      manifestPath,
      manifest,
      platformKey: matchedEntry.platformKey,
      archKey: matchedEntry.archKey,
      libs,
      libPaths,
      stageMode: matchedEntry.stageMode,
      nodeModulesPath: packageNameToNodeModulesPath(dependencyName),
      warnings: libraryWarnings,
    });
  }

  return {
    root,
    platform,
    arch,
    libraries,
    warnings,
  };
}

export function createLynxtronAutoLinkFileSets(
  resolution: LynxtronAutoLinkResolution
): LynxtronAutoLinkFileSet[] {
  return resolution.libraries.map((library) => {
    const filters =
      library.stageMode === 'package'
        ? ['package.json', 'lynx.lib.json', '**/*']
        : [
            'package.json',
            'lynx.lib.json',
            ...library.libs.map(normalizeFilterPath),
          ];

    return {
      from: library.packageRoot,
      to: library.nodeModulesPath,
      filter: Array.from(new Set(filters)),
    };
  });
}

export function createLynxtronAutoLinkStagedLibraries(
  resolution: LynxtronAutoLinkResolution,
  nativeOutputDir = DEFAULT_LYNXTRON_NATIVE_OUTPUT_DIR
): LynxtronAutoLinkStagedLibrary[] {
  const outputRoot = normalizeStagedPath(nativeOutputDir);
  const stagedLibraries: LynxtronAutoLinkStagedLibrary[] = [];

  for (const library of resolution.libraries) {
    if (library.stageMode === 'package') {
      const stagedPackagePath = path.posix.join(
        outputRoot,
        packageNameToNodeModulesPath(library.name)
      );

      stagedLibraries.push({
        name: library.name,
        sourcePath: library.packageRoot,
        stagedPath: stagedPackagePath,
        requirePath: stagedPackagePath,
        requireSpecifier: getNodeApiPackageSpecifier(library.name),
        copyMode: 'package',
      });
      continue;
    }

    for (const [index, sourcePath] of library.libPaths.entries()) {
      const stagedPath = path.posix.join(
        outputRoot,
        packageNameToNodeModulesPath(library.name),
        normalizeFilterPath(library.libs[index])
      );

      stagedLibraries.push({
        name: library.name,
        sourcePath,
        stagedPath,
        requirePath: stagedPath,
        copyMode: 'file',
      });
    }
  }

  return stagedLibraries;
}

export function generateLynxtronAutoLinkCode(
  resolution: LynxtronAutoLinkResolution,
  options: LynxtronAutoLinkCodegenOptions = {}
): string {
  if (options.stagedLibraries !== undefined) {
    return generateLynxtronAutoLinkStagedCode(options.stagedLibraries);
  }

  if (resolution.libraries.length === 0) {
    return 'export {};\n';
  }

  const records = resolution.libraries.map(
    (library) =>
      `  { packageName: ${JSON.stringify(library.name)}, libs: ${JSON.stringify(
        library.libs
      )}, stageMode: ${JSON.stringify(
        library.stageMode
      )}, specifier: ${JSON.stringify(
        library.stageMode === 'package'
          ? getNodeApiPackageSpecifier(library.name)
          : undefined
      )} },`
  );

  return `import __fs from 'node:fs';
import { createRequire as __createRequire } from 'node:module';
import __path from 'node:path';

const __lynxtronAutoLinkRequire = typeof __non_webpack_require__ === 'function'
  ? __non_webpack_require__
  : __createRequire(import.meta.url);
const __lynxtronAutoLinkNodeApiAddons = new Set();

const __lynxtronAutoLinkLibraries = [
${records.join('\n')}
];

function __lynxtronAutoLinkResolvePackageRoot(packageName) {
  try {
    return __path.dirname(
      __lynxtronAutoLinkRequire.resolve(\`\${packageName}/package.json\`),
    );
  } catch {}

  const entryPath = __lynxtronAutoLinkRequire.resolve(packageName);
  let current = __fs.statSync(entryPath).isDirectory()
    ? entryPath
    : __path.dirname(entryPath);

  while (true) {
    const packageJsonPath = __path.join(current, 'package.json');

    if (__fs.existsSync(packageJsonPath)) {
      try {
        const packageJson = JSON.parse(
          __fs.readFileSync(packageJsonPath, 'utf8'),
        );

        if (packageJson && packageJson.name === packageName) {
          return current;
        }
      } catch {}
    }

    const parent = __path.dirname(current);

    if (parent === current) {
      throw new Error(
        \`Cannot resolve AutoLink package root for \${packageName}\`,
      );
    }

    current = parent;
  }
}

function __lynxtronAutoLinkLoadNodeApiAddon(libraryPath, isPackageEntry) {
  const loadPath = isPackageEntry
    ? __lynxtronAutoLinkRequire.resolve(libraryPath)
    : __path.resolve(libraryPath);

  if (__lynxtronAutoLinkNodeApiAddons.has(loadPath)) {
    return;
  }

  __lynxtronAutoLinkRequire(isPackageEntry ? libraryPath : loadPath);
  __lynxtronAutoLinkNodeApiAddons.add(loadPath);
}

for (const __lynxtronAutoLinkRecord of __lynxtronAutoLinkLibraries) {
  let __lynxtronAutoLinkPackageRoot;

  if (__lynxtronAutoLinkRecord.stageMode === 'package') {
    __lynxtronAutoLinkLoadNodeApiAddon(
      __lynxtronAutoLinkRecord.specifier,
      true,
    );
    continue;
  }

  for (const __lynxtronAutoLinkLib of __lynxtronAutoLinkRecord.libs) {
    __lynxtronAutoLinkPackageRoot ??= __lynxtronAutoLinkResolvePackageRoot(
      __lynxtronAutoLinkRecord.packageName,
    );
    __lynxtronAutoLinkLoadNodeApiAddon(
      __path.join(__lynxtronAutoLinkPackageRoot, __lynxtronAutoLinkLib),
      false,
    );
  }
}
`;
}

export function writeLynxtronAutoLinkModule(
  resolution: LynxtronAutoLinkResolution,
  filePath: string,
  options: LynxtronAutoLinkCodegenOptions = {}
): string {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, generateLynxtronAutoLinkCode(resolution, options));
  return filePath;
}

function generateLynxtronAutoLinkStagedCode(
  stagedLibraries: LynxtronAutoLinkStagedLibrary[]
): string {
  if (stagedLibraries.length === 0) {
    return 'export {};\n';
  }

  const records = stagedLibraries.map(
    (library) =>
      `  { copyMode: ${JSON.stringify(
        library.copyMode
      )}, path: ${JSON.stringify(
        normalizeFilterPath(library.requirePath)
      )}, packageRoot: ${JSON.stringify(
        normalizeFilterPath(library.stagedPath)
      )}, packageName: ${JSON.stringify(
        library.name
      )}, specifier: ${JSON.stringify(library.requireSpecifier)} },`
  );

  return `import __fs from 'node:fs';
import __module, { createRequire as __createRequire } from 'node:module';
import __path from 'node:path';
import { fileURLToPath as __fileURLToPath } from 'node:url';

const __lynxtronAutoLinkRequire = typeof __non_webpack_require__ === 'function'
  ? __non_webpack_require__
  : __createRequire(import.meta.url);
const __lynxtronAutoLinkSourceDir = __path.dirname(
  __fileURLToPath(import.meta.url),
);
const __lynxtronAutoLinkNodeApiAddons = new Set();
const __lynxtronAutoLinkLibraries = [
${records.join('\n')}
];

function __lynxtronAutoLinkLoadNodeApiAddon(library) {
  const loadPath = __lynxtronAutoLinkResolveExistingLoadPath(
    library.copyMode === 'package' ? library.packageRoot : library.path,
  );

  if (__lynxtronAutoLinkNodeApiAddons.has(loadPath)) {
    return;
  }

  if (library.copyMode === 'package') {
    __lynxtronAutoLinkRegisterNodeModulesRoot(loadPath, library.packageName);
    const packageRequire = __createRequire(__path.join(loadPath, 'package.json'));
    packageRequire(library.specifier);
  } else {
    __lynxtronAutoLinkRequire(loadPath);
  }

  __lynxtronAutoLinkNodeApiAddons.add(loadPath);
}

function __lynxtronAutoLinkRegisterNodeModulesRoot(packageRoot, packageName) {
  let nodeModulesRoot = packageRoot;

  for (const _ of packageName.split('/')) {
    nodeModulesRoot = __path.dirname(nodeModulesRoot);
  }

  if (!__module.globalPaths.includes(nodeModulesRoot)) {
    __module.globalPaths.unshift(nodeModulesRoot);
  }
}

function __lynxtronAutoLinkResolveExistingLoadPath(libraryPath) {
  const attemptedPaths = [];

  for (const runtimeDir of __lynxtronAutoLinkGetRuntimeDirs()) {
    const loadPath = __lynxtronAutoLinkResolveNativeLoadPath(
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

function __lynxtronAutoLinkGetRuntimeDirs() {
  const runtimeDirs = [];
  const pushDir = (dir) => {
    if (typeof dir === 'string' && dir.length > 0) {
      runtimeDirs.push(__path.resolve(dir));
    }
  };

  if (Array.isArray(process.argv) && process.argv.length > 1) {
    pushDir(__lynxtronAutoLinkResolveAppArg(process.argv[1]));
  }

  if (
    typeof process.resourcesPath === 'string' &&
    process.resourcesPath.length > 0
  ) {
    pushDir(__path.join(process.resourcesPath, 'app.asar'));
    pushDir(__path.join(process.resourcesPath, 'app'));
    pushDir(__path.join(process.resourcesPath, 'resources', 'app'));
    pushDir(process.resourcesPath);
  }

  pushDir(__lynxtronAutoLinkSourceDir);
  pushDir(process.cwd());

  return Array.from(new Set(runtimeDirs));
}

function __lynxtronAutoLinkResolveAppArg(value) {
  const appPath = __path.resolve(process.cwd(), value);

  try {
    return __fs.statSync(appPath).isDirectory()
      ? appPath
      : __path.dirname(appPath);
  } catch {
    return appPath;
  }
}

function __lynxtronAutoLinkResolveNativeLoadPath(absolutePath) {
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

for (const __lynxtronAutoLinkLibrary of __lynxtronAutoLinkLibraries) {
  __lynxtronAutoLinkLoadNodeApiAddon(__lynxtronAutoLinkLibrary);
}
`;
}

function collectDependencyNames(
  packageJson: PackageJson,
  dependencyFields: string[]
): string[] {
  const names = new Set<string>();

  for (const field of dependencyFields) {
    const dependencies = packageJson[field as keyof PackageJson];

    if (dependencies === undefined || typeof dependencies !== 'object') {
      continue;
    }

    for (const name of Object.keys(dependencies)) {
      names.add(name);
    }
  }

  return Array.from(names).sort((left, right) => left.localeCompare(right));
}

function resolvePackageRoot(
  root: string,
  packageName: string
): { packageRoot: string; packageJsonPath: string } | undefined {
  const requireFromRoot = createRequire(path.join(root, 'package.json'));

  try {
    const packageJsonPath = requireFromRoot.resolve(
      `${packageName}/package.json`
    );
    return {
      packageRoot: path.dirname(packageJsonPath),
      packageJsonPath,
    };
  } catch {}

  try {
    const entryPath = requireFromRoot.resolve(packageName);
    return findPackageRoot(entryPath, packageName);
  } catch {
    return undefined;
  }
}

function findPackageRoot(
  startPath: string,
  packageName: string
): { packageRoot: string; packageJsonPath: string } | undefined {
  let current = fs.statSync(startPath).isDirectory()
    ? startPath
    : path.dirname(startPath);

  while (true) {
    const packageJsonPath = path.join(current, 'package.json');
    const packageJson = readJson<PackageJson>(packageJsonPath);

    if (packageJson?.name === packageName) {
      return {
        packageRoot: current,
        packageJsonPath,
      };
    }

    const parent = path.dirname(current);

    if (parent === current) {
      return undefined;
    }

    current = parent;
  }
}

function getNodeApiManifestEntry(
  manifest: Record<string, unknown>,
  platform: string,
  arch: string
): MatchedManifestEntry | undefined {
  const platforms = manifest.platforms;

  if (platforms === null || typeof platforms !== 'object') {
    return undefined;
  }

  const platformRecords = platforms as Record<string, unknown>;

  const nodeApiEntry = matchNodeApiManifestEntry(
    platformRecords['lynxtron'],
    platform,
    arch
  );

  if (nodeApiEntry !== undefined) {
    return nodeApiEntry;
  }

  for (const platformKey of getManifestKeys(platform, PLATFORM_ALIASES)) {
    const matchedEntry = matchBinaryManifestEntry(
      platformKey,
      platformRecords[platformKey],
      platform,
      arch
    );

    if (matchedEntry !== undefined) {
      return matchedEntry;
    }
  }

  return undefined;
}

function matchNodeApiManifestEntry(
  platformEntry: unknown,
  runtimePlatform: string,
  runtimeArch: string
): MatchedManifestEntry | undefined {
  if (platformEntry === null || typeof platformEntry !== 'object') {
    return undefined;
  }

  const nodeApiEntry = platformEntry as LynxNodeApiManifestEntry;
  const paths = normalizeLibraryPaths(nodeApiEntry.path);

  if (paths.length > 0) {
    return {
      platformKey: 'lynxtron',
      archKey: runtimeArch,
      libs: paths,
      stageMode: 'package',
    };
  }

  return matchBinaryManifestEntry(
    'lynxtron',
    platformEntry,
    runtimePlatform,
    runtimeArch
  );
}

function matchBinaryManifestEntry(
  platformKey: string,
  platformEntry: unknown,
  runtimePlatform: string,
  runtimeArch: string
): MatchedManifestEntry | undefined {
  if (platformEntry === null || typeof platformEntry !== 'object') {
    return undefined;
  }

  const desktopEntry = platformEntry as LynxDesktopManifestEntry;

  for (const binaryEntry of normalizeBinaryEntries(desktopEntry.binary)) {
    const os = readOptionalString(binaryEntry.os);
    const binaryArch =
      readOptionalString(binaryEntry.arch) ??
      readOptionalString(binaryEntry.arc);
    const binaryPaths = normalizeLibraryPaths(binaryEntry.path);

    if (
      os === undefined ||
      binaryArch === undefined ||
      binaryPaths.length === 0 ||
      !matchesManifestKey(runtimePlatform, os, PLATFORM_ALIASES) ||
      !matchesManifestKey(runtimeArch, binaryArch, ARCH_ALIASES)
    ) {
      continue;
    }

    return {
      platformKey,
      archKey: binaryArch,
      libs: binaryPaths,
      stageMode: 'file',
    };
  }

  return undefined;
}

function normalizeBinaryEntries(value: unknown): LynxNodeApiBinaryEntry[] {
  const values = Array.isArray(value) ? value : [value];
  const entries: LynxNodeApiBinaryEntry[] = [];

  for (const item of values) {
    if (item !== null && typeof item === 'object') {
      entries.push(item as LynxNodeApiBinaryEntry);
    }
  }

  return entries;
}

function getManifestKeys(
  value: string,
  aliases: Record<string, string[]>
): string[] {
  return Array.from(new Set(aliases[value] ?? [value]));
}

function matchesManifestKey(
  runtimeValue: string,
  manifestValue: string,
  aliases: Record<string, string[]>
): boolean {
  return getManifestKeys(runtimeValue, aliases).includes(manifestValue);
}

function normalizeLibraryPaths(value: unknown): string[] {
  const values = Array.isArray(value) ? value : [value];
  const paths: string[] = [];

  for (const item of values) {
    const libraryPath = normalizeRelativePath(item);

    if (libraryPath !== undefined) {
      paths.push(libraryPath);
    }
  }

  return paths;
}

function expandManifestVariables(
  value: string,
  platform: string,
  arch: string,
  manifestPlatform: string,
  manifestArch: string
): string {
  return value
    .replaceAll('${platform}', platform)
    .replaceAll('${arch}', arch)
    .replaceAll('${manifestPlatform}', manifestPlatform)
    .replaceAll('${manifestArch}', manifestArch);
}

function packageNameToNodeModulesPath(packageName: string): string {
  return `node_modules/${packageName}`;
}

function getNodeApiPackageSpecifier(packageName: string): string {
  return `${packageName}/lynxtron`;
}

function normalizeRelativePath(value: unknown): string | undefined {
  if (typeof value !== 'string' || value.length === 0) {
    return undefined;
  }

  return value.replaceAll('\\', '/').replace(/^\.\//, '');
}

function readOptionalString(value: unknown): string | undefined {
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

function normalizeFilterPath(value: string): string {
  return value.replaceAll('\\', '/').replace(/^\.\//, '');
}

function normalizeStagedPath(value: string): string {
  const normalizedPath = normalizeFilterPath(value);
  return (
    normalizedPath.replace(/\/$/, '') || DEFAULT_LYNXTRON_NATIVE_OUTPUT_DIR
  );
}

function hasGlob(value: string): boolean {
  return value.includes('*');
}

function readJson<T>(filePath: string): T | undefined {
  try {
    return JSON.parse(fs.readFileSync(filePath, 'utf8')) as T;
  } catch {
    return undefined;
  }
}
