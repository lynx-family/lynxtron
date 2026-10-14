import { getPostinstallRuntimeOptions, shouldSkipPostinstallRuntime } from './install-policy.js';
import { ensureRuntime } from './runtime-manager.js';
import fs from 'node:fs';
const manifest = JSON.parse(fs.readFileSync(new URL('./package.json', import.meta.url), 'utf8'));

if (shouldSkipPostinstallRuntime()) {
  console.log('Skipping Lynxtron runtime download for source build');
  process.exit(0);
}

const options = getPostinstallRuntimeOptions();
const { customUrl } = options;
if (!customUrl && !manifest.optionalDependencies) {
  console.log('Source checkout; skipping runtime installation');
} else {
  await ensureRuntime({ ...options, allowFetch: false });
}
