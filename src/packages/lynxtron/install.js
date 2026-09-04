import {
  getPostinstallRuntimeOptions,
  shouldSkipPostinstallRuntime,
} from './install-policy.js';
import { ensureRuntime } from './runtime-manager.js';

if (!shouldSkipPostinstallRuntime()) {
  const options = getPostinstallRuntimeOptions();
  await ensureRuntime(options);
}
