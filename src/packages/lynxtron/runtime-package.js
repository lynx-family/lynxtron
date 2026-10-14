import fs from 'node:fs/promises';
import path from 'node:path';
import { createRequire } from 'node:module';
import Config from '@npmcli/config';
import configDefinitions from '@npmcli/config/lib/definitions/index.js';
import pacote from 'pacote';
import runtimeArtifacts from './runtime-artifacts.cjs';

const { definitions, flatten, shorthands } = configDefinitions;
const { getRuntimePackageName } = runtimeArtifacts;

export async function loadRegistryOptions(cwd = process.cwd()) {
  const config = new Config({ definitions, flatten, shorthands,
    npmPath: path.dirname(createRequire(import.meta.url).resolve('@npmcli/config/package.json')),
    cwd, argv: [] });
  await config.load();
  return { ...config.flat, ignoreScripts: true };
}

export async function copyRuntimePackageArchive({ platform, arch, variant, version,
  packageRoot, archivePath, allowFetch = false, cwd = process.cwd() }) {
  const name = getRuntimePackageName({ platform, arch, variant });
  const require = createRequire(path.join(packageRoot, 'package.json'));
  let manifestPath;
  try { manifestPath = require.resolve(`${name}/package.json`); }
  catch (error) { if (error.code !== 'MODULE_NOT_FOUND') throw error; }
  if (manifestPath) {
    const manifest = JSON.parse(await fs.readFile(manifestPath, 'utf8'));
    if (manifest.version === version) {
      await fs.copyFile(path.join(path.dirname(manifestPath), 'runtime.zip'), archivePath);
      return;
    }
  }
  if (!allowFetch) {
    throw new Error(`Missing ${name}@${version}. Reinstall with optional dependencies enabled and check that your npm mirror has synced this version.`);
  }
  const directory = await fs.mkdtemp(`${archivePath}-package-`);
  try {
    const options = await loadRegistryOptions(cwd);
    await pacote.extract(`${name}@${version}`, directory, options);
    const manifest = JSON.parse(await fs.readFile(path.join(directory, 'package.json'), 'utf8'));
    if (manifest.name !== name || manifest.version !== version) {
      throw new Error(`Unexpected runtime package identity for ${name}@${version}`);
    }
    await fs.copyFile(path.join(directory, 'runtime.zip'), archivePath);
  } finally {
    await fs.rm(directory, { recursive: true, force: true });
  }
}
