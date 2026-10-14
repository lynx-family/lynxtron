import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import lockfile from 'proper-lockfile';
import zipLib from 'zip-lib';

import { copyRuntimePackageArchive } from './runtime-package.js';
import runtimeArtifacts from './runtime-artifacts.cjs';
import { downloadBinary } from './utils/download.js';
import {
  ARCH,
  BASE_URL,
  PLATFORM,
  PLATFROM_EXE_PATH,
  VERSION,
} from './utils/env-config.js';

const { getRuntimeArtifactFilename, normalizeRuntimeVariant } = runtimeArtifacts;
const { extract: extractZip } = zipLib;
const __dirname = path.dirname(fileURLToPath(import.meta.url));

export async function extractRuntimeArchive(archivePath, { dir }) {
  await fs.promises.mkdir(dir, { recursive: true });
  const realDirectory = await fs.promises.realpath(dir);
  await extractZip(archivePath, realDirectory, { safeSymlinksOnly: true });
}

export function getRuntimeDirectory(variant, packageRoot = __dirname) {
  return path.join(packageRoot, 'dist', normalizeRuntimeVariant(variant));
}

export function getRuntimeExecutablePath(variant, packageRoot = __dirname) {
  return path.join(getRuntimeDirectory(variant, packageRoot), PLATFROM_EXE_PATH);
}

export function getRuntimeDownloadUrl({
  baseUrl = BASE_URL,
  version = VERSION,
  platform = PLATFORM,
  arch = ARCH,
  variant,
} = {}) {
  if (!baseUrl) {
    return '';
  }
  const filename = getRuntimeArtifactFilename({
    version,
    platform,
    arch,
    variant,
  });
  return `${baseUrl.replace(/\/+$/, '')}/v${version}/${filename}`;
}

export async function ensureRuntime({
  variant,
  customUrl,
  force = false,
  timeoutMs = 120000,
  packageRoot = __dirname,
  executableRelativePath = PLATFROM_EXE_PATH,
  platform = PLATFORM,
  arch = ARCH,
  version = VERSION,
  allowFetch = variant === 'release',
  registryCwd = process.cwd(),
  packageArchive = copyRuntimePackageArchive,
  download = downloadBinary,
  extract = extractRuntimeArchive,
} = {}) {
  const normalizedVariant = normalizeRuntimeVariant(variant, 'devtool');
  const runtimeDirectory = getRuntimeDirectory(normalizedVariant, packageRoot);
  const executablePath = path.join(runtimeDirectory, executableRelativePath);

  if (!force && fs.existsSync(executablePath)) {
    return { downloaded: false, executablePath, variant: normalizedVariant };
  }

  const distRoot = path.join(packageRoot, 'dist');
  await fs.promises.mkdir(distRoot, { recursive: true });
  const lockTarget = path.join(distRoot, `.install-${normalizedVariant}`);
  const releaseLock = await lockfile.lock(lockTarget, {
    realpath: false,
    stale: Math.max(timeoutMs, 120000),
    update: 10000,
    retries: {
      retries: 120,
      factor: 1.2,
      minTimeout: 50,
      maxTimeout: 1000,
      randomize: true,
    },
  });

  try {
    if (!force && fs.existsSync(executablePath)) {
      return { downloaded: false, executablePath, variant: normalizedVariant };
    }

    const temporaryDirectory = await fs.promises.mkdtemp(
      path.join(distRoot, `.${normalizedVariant}-`)
    );
    const archivePath = `${temporaryDirectory}.zip`;


    try {
      if (customUrl) {
        await download(customUrl, archivePath, { timeoutMs });
      } else {
        await packageArchive({ platform, arch, version, variant: normalizedVariant,
          packageRoot, archivePath, allowFetch, cwd: registryCwd });
      }
      await extract(archivePath, { dir: temporaryDirectory });

      const temporaryExecutable = path.join(temporaryDirectory, executableRelativePath);
      if (!fs.existsSync(temporaryExecutable)) {
        throw new Error(
          `Expected executable ${temporaryExecutable} was not found in the runtime archive`
        );
      }
      if (platform === 'darwin' || platform === 'linux') {
        await fs.promises.chmod(temporaryExecutable, 0o755);
      }

      await fs.promises.rm(runtimeDirectory, { recursive: true, force: true });
      await fs.promises.rename(temporaryDirectory, runtimeDirectory);
      console.log(`Lynxtron ${normalizedVariant} runtime installed successfully`);
      return { downloaded: true, executablePath, variant: normalizedVariant };
    } finally {
      await fs.promises.rm(archivePath, { force: true });
      await fs.promises.rm(temporaryDirectory, { recursive: true, force: true });
    }
  } finally {
    await releaseLock();
  }
}

// Builder caches target runtimes separately from the host development runtime.
export async function ensureTargetRuntime({ platform, arch, version = VERSION,
  variant = 'release', cwd = process.cwd(), cacheRoot } = {}) {
  runtimeArtifacts.getRuntimePackageName({ platform, arch, variant });
  if (!cacheRoot) {
    const { loadRegistryOptions } = await import('./runtime-package.js');
    const options = await loadRegistryOptions(cwd);
    cacheRoot = path.join(options.cache, 'lynxtron-runtimes');
  }
  if (!/^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/.test(version)) {
    throw new Error(`Invalid runtime version: ${version}`);
  }
  const packageRoot = path.join(cacheRoot, version, `${platform}-${arch}`);
  await ensureRuntime({ platform, arch, version, variant, packageRoot,
    executableRelativePath: runtimeArtifacts.getRuntimeExecutableRelativePath(platform),
    allowFetch: true, registryCwd: cwd });
  return getRuntimeDirectory(variant, packageRoot);
}
