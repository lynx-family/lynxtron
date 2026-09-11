import { getPostinstallRuntimeOptions, shouldSkipPostinstallRuntime } from './install-policy.js';
import { ensureRuntime } from './runtime-manager.js';
import { BASE_URL } from './utils/env-config.js';

if (shouldSkipPostinstallRuntime()) {
  console.log('Skipping Lynxtron runtime download for source build');
  process.exit(0);
}

const options = getPostinstallRuntimeOptions();
const { customUrl } = options;
if (!customUrl && !BASE_URL) {
  console.log('Lynxtron base URL is empty; skipping runtime download');
} else {
  await ensureRuntime(options);
}
