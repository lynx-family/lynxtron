import test from 'node:test';
import assert from 'node:assert/strict';

import {
  POSTINSTALL_RUNTIME_VARIANT,
  getPostinstallRuntimeOptions,
  shouldSkipPostinstallRuntime,
} from './install-policy.js';

test('npm postinstall always installs the DevTool runtime', () => {
  assert.equal(POSTINSTALL_RUNTIME_VARIANT, 'devtool');
  assert.deepEqual(
    getPostinstallRuntimeOptions({
      LYNXTRON_RUNTIME_VARIANT: 'release',
      LYNXTRON_BINARY_URL: 'https://downloads.example.test/devtool.zip',
      npm_config_force_download: 'true',
    }),
    {
      variant: 'devtool',
      customUrl: 'https://downloads.example.test/devtool.zip',
      force: true,
    }
  );
});

test('source builds can skip the unpublished postinstall runtime', () => {
  assert.equal(shouldSkipPostinstallRuntime({}), false);
  assert.equal(shouldSkipPostinstallRuntime({ LYNXTRON_SKIP_DOWNLOAD: '0' }), false);
  assert.equal(shouldSkipPostinstallRuntime({ LYNXTRON_SKIP_DOWNLOAD: '1' }), true);
  assert.equal(
    shouldSkipPostinstallRuntime({ LYNXTRON_SKIP_DOWNLOAD: 'true' }),
    true
  );
});
