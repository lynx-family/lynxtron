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

export interface LynxNodeApiManifestEntry {
  targets?: LynxtronRuntimeTarget[];
}

export interface LynxtronRuntimeTarget {
  os: string;
  arch: string;
  files?: string[];
  frameworks?: string[];
  appBundles?: string[];
}

export interface LynxtronAutoLinkLibrary {
  name: string;
  packageRoot: string;
  packageJsonPath: string;
  manifestPath: string;
  manifest: Record<string, unknown>;
  platformKey: string;
  archKey: string;
  files: string[];
  filePaths: string[];
  frameworks: string[];
  frameworkPaths: string[];
  appBundles: string[];
  appBundlePaths: string[];
  entry: string;
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
  requireSpecifier: string;
  files: string[];
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
  files: string[];
  frameworks: string[];
  appBundles: string[];
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

    const files = matchedEntry.files.map((filePath) =>
      expandManifestVariables(
        filePath,
        platform,
        arch,
        matchedEntry.platformKey,
        matchedEntry.archKey
      )
    );

    const filePaths = files.map((filePath) =>
      path.join(resolvedPackage.packageRoot, filePath)
    );
    const frameworks = matchedEntry.frameworks.map((frameworkPath) =>
      expandManifestVariables(
        frameworkPath,
        platform,
        arch,
        matchedEntry.platformKey,
        matchedEntry.archKey
      )
    );
    const frameworkPaths = frameworks.map((frameworkPath) =>
      path.join(resolvedPackage.packageRoot, frameworkPath)
    );
    const appBundles = matchedEntry.appBundles.map((appBundlePath) =>
      expandManifestVariables(
        appBundlePath,
        platform,
        arch,
        matchedEntry.platformKey,
        matchedEntry.archKey
      )
    );
    const appBundlePaths = appBundles.map((appBundlePath) =>
      path.join(resolvedPackage.packageRoot, appBundlePath)
    );

    if (
      files.length === 0 &&
      frameworks.length === 0 &&
      appBundles.length === 0
    ) {
      continue;
    }
    const libraryWarnings: string[] = [];
    const requireFromPackage = createRequire(resolvedPackage.packageJsonPath);
    const lynxtronSpecifier = getNodeApiPackageSpecifier(dependencyName);
    let entry = '';

    try {
      const entryPath = requireFromPackage.resolve(lynxtronSpecifier);
      const relativeEntry = path.relative(
        resolvedPackage.packageRoot,
        entryPath
      );
      if (relativeEntry.startsWith('..') || path.isAbsolute(relativeEntry)) {
        libraryWarnings.push(
          `Lynxtron AutoLink package "${dependencyName}" resolves ${lynxtronSpecifier} outside its package.`
        );
      } else {
        entry = normalizeFilterPath(relativeEntry);
      }
    } catch {
      libraryWarnings.push(
        `Lynxtron AutoLink package "${dependencyName}" declares Lynxtron assets, but ${lynxtronSpecifier} cannot be resolved.`
      );
    }

    validateArtifactPaths({
      dependencyName,
      kind: 'file',
      paths: files,
      absolutePaths: filePaths,
      warnings: libraryWarnings,
    });
    validateArtifactPaths({
      dependencyName,
      kind: 'Framework',
      paths: frameworks,
      absolutePaths: frameworkPaths,
      warnings: libraryWarnings,
    });
    validateArtifactPaths({
      dependencyName,
      kind: 'app bundle',
      paths: appBundles,
      absolutePaths: appBundlePaths,
      warnings: libraryWarnings,
    });

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
      files,
      filePaths,
      frameworks,
      frameworkPaths,
      appBundles,
      appBundlePaths,
      entry,
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
  return resolution.libraries.map((library) => ({
    from: library.packageRoot,
    to: library.nodeModulesPath,
    filter: getAutoLinkPackageFiles(library),
  }));
}

export function createLynxtronAutoLinkStagedLibraries(
  resolution: LynxtronAutoLinkResolution,
  nativeOutputDir = DEFAULT_LYNXTRON_NATIVE_OUTPUT_DIR
): LynxtronAutoLinkStagedLibrary[] {
  const outputRoot = normalizeStagedPath(nativeOutputDir);
  const stagedLibraries: LynxtronAutoLinkStagedLibrary[] = [];

  for (const library of resolution.libraries) {
    const stagedPackagePath = path.posix.join(
      outputRoot,
      packageNameToNodeModulesPath(library.name)
    );

    stagedLibraries.push({
      name: library.name,
      sourcePath: library.packageRoot,
      stagedPath: stagedPackagePath,
      requireSpecifier: getNodeApiPackageSpecifier(library.name),
      files: getAutoLinkPackageFiles(library),
    });
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

  const specifiers = resolution.libraries.map((library) =>
    getNodeApiPackageSpecifier(library.name)
  );

  return `import { createRequire as __createRequire } from 'node:module';

const __lynxtronAutoLinkRequire = typeof __non_webpack_require__ === 'function'
  ? __non_webpack_require__
  : __createRequire(
      typeof __filename === 'string' ? __filename : import.meta.url,
    );
const __lynxtronAutoLinkNodeApiAddons = new Set();
const __lynxtronAutoLinkLibraries = ${JSON.stringify(specifiers)};

function __lynxtronAutoLinkLoadNodeApiAddon(specifier) {
  const loadPath = __lynxtronAutoLinkRequire.resolve(specifier);

  if (__lynxtronAutoLinkNodeApiAddons.has(loadPath)) {
    return;
  }

  __lynxtronAutoLinkRequire(specifier);
  __lynxtronAutoLinkNodeApiAddons.add(loadPath);
}

for (const __lynxtronAutoLinkSpecifier of __lynxtronAutoLinkLibraries) {
  __lynxtronAutoLinkLoadNodeApiAddon(__lynxtronAutoLinkSpecifier);
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
      `  { packageRoot: ${JSON.stringify(
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
  : __createRequire(
      typeof __filename === 'string' ? __filename : import.meta.url,
    );
const __lynxtronAutoLinkSourceDir = __path.dirname(
  typeof __filename === 'string'
    ? __filename
    : __fileURLToPath(import.meta.url),
);
const __lynxtronAutoLinkNodeApiAddons = new Set();
const __lynxtronAutoLinkLibraries = [
${records.join('\n')}
];

function __lynxtronAutoLinkLoadNodeApiAddon(library) {
  const loadPath = __lynxtronAutoLinkResolveExistingLoadPath(
    library.packageRoot,
  );

  if (__lynxtronAutoLinkNodeApiAddons.has(loadPath)) {
    return;
  }

  __lynxtronAutoLinkRegisterNodeModulesRoot(loadPath, library.packageName);
  const packageRequire = __createRequire(__path.join(loadPath, 'package.json'));
  packageRequire(library.specifier);

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

  return matchNodeApiManifestEntry(
    (platforms as Record<string, unknown>)['lynxtron'],
    platform,
    arch
  );
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
  const target = matchRuntimeTarget(
    nodeApiEntry.targets,
    runtimePlatform,
    runtimeArch
  );
  if (target === undefined) {
    return undefined;
  }

  if (
    'binary' in target.entry ||
    'binaries' in target.entry ||
    'resources' in target.entry
  ) {
    throw new Error(
      'Lynxtron AutoLink does not support binary, binaries, or resources in targets; use files.'
    );
  }

  const frameworks = normalizeLibraryPaths(target.entry.frameworks);
  if (frameworks.length > 0) {
    if (!matchesManifestKey('darwin', runtimePlatform, PLATFORM_ALIASES)) {
      throw new Error(
        'Lynxtron AutoLink only supports frameworks for darwin targets.'
      );
    }
    if (frameworks.some((framework) => !framework.endsWith('.framework'))) {
      throw new Error(
        'Lynxtron AutoLink requires every frameworks path to end in .framework.'
      );
    }
  }

  const appBundles = normalizeLibraryPaths(target.entry.appBundles);
  if (appBundles.length > 0) {
    if (!matchesManifestKey('darwin', runtimePlatform, PLATFORM_ALIASES)) {
      throw new Error(
        'Lynxtron AutoLink only supports appBundles for darwin targets.'
      );
    }
    if (appBundles.some((appBundle) => !appBundle.endsWith('.app'))) {
      throw new Error(
        'Lynxtron AutoLink requires every appBundles path to end in .app.'
      );
    }
  }

  return {
    platformKey: 'lynxtron',
    archKey: target.archKey,
    files: normalizeLibraryPaths(target.entry.files),
    frameworks,
    appBundles,
  };
}

function matchRuntimeTarget(
  targets: unknown,
  runtimePlatform: string,
  runtimeArch: string
): { archKey: string; entry: LynxtronRuntimeTarget } | undefined {
  const matches: Array<{
    archKey: string;
    entry: LynxtronRuntimeTarget;
  }> = [];

  for (const target of normalizeRuntimeTargets(targets)) {
    const os = readOptionalString(target.os);
    const targetArch = readOptionalString(target.arch);

    if (
      os === undefined ||
      targetArch === undefined ||
      !matchesManifestKey(runtimePlatform, os, PLATFORM_ALIASES) ||
      !matchesManifestKey(runtimeArch, targetArch, ARCH_ALIASES)
    ) {
      continue;
    }

    matches.push({
      archKey: targetArch,
      entry: target,
    });
  }

  if (matches.length > 1) {
    throw new Error(
      `Lynxtron AutoLink found duplicate targets for ${runtimePlatform}/${runtimeArch}.`
    );
  }

  return matches[0];
}

function normalizeRuntimeTargets(value: unknown): LynxtronRuntimeTarget[] {
  if (!Array.isArray(value)) {
    return [];
  }

  const entries: LynxtronRuntimeTarget[] = [];

  for (const item of value) {
    if (item !== null && typeof item === 'object') {
      entries.push(item as LynxtronRuntimeTarget);
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

function validateArtifactPaths({
  dependencyName,
  kind,
  paths,
  absolutePaths,
  warnings,
}: {
  dependencyName: string;
  kind: string;
  paths: string[];
  absolutePaths: string[];
  warnings: string[];
}): void {
  for (const [index, absolutePath] of absolutePaths.entries()) {
    if (hasGlob(paths[index])) {
      warnings.push(
        `Lynxtron AutoLink package "${dependencyName}" declares unsupported ${kind} glob ${paths[index]}.`
      );
      continue;
    }

    if (!fs.existsSync(absolutePath)) {
      warnings.push(
        `Lynxtron AutoLink package "${dependencyName}" declares ${kind} assets ${paths[index]}, but the path does not exist.`
      );
    }
  }
}

function getAutoLinkPackageFiles(library: LynxtronAutoLinkLibrary): string[] {
  return Array.from(
    new Set(
      [
        'package.json',
        'lynx.lib.json',
        library.entry,
        ...library.files,
        ...library.frameworks,
        ...library.appBundles,
      ]
        .filter((entry) => entry.length > 0 && !hasGlob(entry))
        .map(normalizeFilterPath)
    )
  );
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

  const normalizedPath = value.replaceAll('\\', '/').replace(/^\.\//, '');
  if (
    path.posix.isAbsolute(normalizedPath) ||
    /^[A-Za-z]:\//.test(normalizedPath) ||
    normalizedPath.split('/').includes('..')
  ) {
    return undefined;
  }

  return normalizedPath;
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
