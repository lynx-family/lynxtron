import { getPostinstallRuntimeOptions, shouldSkipPostinstallRuntime } from './install-policy.js';
import { ensureRuntime } from './runtime-manager.js';

if (shouldSkipPostinstallRuntime()) {
  console.log('Skipping Lynxtron runtime download for source build');
  process.exit(0);
}

const options = getPostinstallRuntimeOptions();
await ensureRuntime(options);
