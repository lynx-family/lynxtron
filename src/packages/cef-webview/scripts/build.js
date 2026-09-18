import { spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';

const require = createRequire(import.meta.url);
const arch = process.env.npm_config_arch || process.arch;
const args = [
  require.resolve('cmake-js/bin/cmake-js'),
  'rebuild',
  '--arch',
  arch,
];

if (process.platform === 'win32') {
  const preference = process.env.LYNXTRON_PREFER_DISCRETE_GPU ?? 'true';
  if (!['true', 'false'].includes(preference)) {
    throw new Error('LYNXTRON_PREFER_DISCRETE_GPU must be true or false');
  }
  args.push(
    `--CDLYNXTRON_PREFER_DISCRETE_GPU=${preference === 'true' ? 'ON' : 'OFF'}`
  );
}

// Source builds provide the import library from this build, not a previously
// downloaded npm runtime. CMake variables are not inherited from the environment.
const importLibrary = process.env.LYNXTRON_IMPORT_LIB;
if (importLibrary !== undefined) {
  if (
    !path.isAbsolute(importLibrary) ||
    !fs.statSync(importLibrary, { throwIfNoEntry: false })?.isFile()
  ) {
    throw new Error(
      `LYNXTRON_IMPORT_LIB must point to an existing absolute file: ${importLibrary}`
    );
  }
  args.push(`--CDLYNXTRON_IMPORT_LIB=${importLibrary}`);
}

// The host Node architecture need not match the native build target.
// Pass the target explicitly: environment-only CMake settings are overridden
// by cmake-js's default (the host Node architecture).
const result = spawnSync(process.execPath, args, { stdio: 'inherit' });
if (result.error) throw result.error;
process.exit(result.status ?? 1);
