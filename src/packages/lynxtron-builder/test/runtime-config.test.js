const test = require('node:test');
const assert = require('node:assert/strict');
const { validateConfiguration } = require('app-builder-lib/out/util/config/config.js');

const runtimeArtifacts = require('../../lynxtron/runtime-artifacts.cjs');
const {
  applyArchitectureToConfig,
  isUniversalFromConfig,
  prepareRuntimeConfig,
  prepareUniversalPackaging,
  resolveBuildPlan,
  resolveConfiguredArchitectures,
} = require('../runtime-config.js');

function prepare({ config = {}, args = [], env = {}, arch } = {}) {
  return prepareRuntimeConfig({
    config,
    args,
    env,
    arch,
    platform: 'darwin',
    defaultArch: 'arm64',
    lynxtronVersion: '1.2.3',
    runtimeArtifacts,
  });
}

test('builder defaults to the release runtime', () => {
  const result = prepare();
  assert.equal(result.variant, 'release');
  assert.equal(
    result.config.electronDownload.mirror,
    'https://github.com/lynx-family/lynxtron/releases/download/'
  );
  assert.equal(result.config.electronDownload.customFilename, 'lynxtron-v1.2.3-darwin-arm64.zip');
});

test('electron-builder yml can select DevTool and custom config is stripped', () => {
  const result = prepare({ config: { lynxtron: { runtimeVariant: 'devtool' } } });
  assert.equal(result.variant, 'devtool');
  assert.equal(result.config.lynxtron, undefined);
  assert.equal(result.config.electronDownload.customFilename, 'lynxtron-v1.2.3-darwin-arm64-devtool.zip');
});

test('CLI overrides environment and yml and is not forwarded', () => {
  const result = prepare({
    config: { lynxtron: { runtimeVariant: 'devtool' } },
    args: ['--mac', '--lynxtron-runtime', 'release', '--x64'],
    env: { LYNXTRON_RUNTIME_VARIANT: 'devtool' },
  });
  assert.equal(result.variant, 'release');
  assert.deepEqual(result.forwardedArgs, ['--mac', '--x64']);
  assert.equal(result.config.electronDownload.customFilename, 'lynxtron-v1.2.3-darwin-x64.zip');
});

test('environment overrides yml and supports DevTool MAS artifacts', () => {
  const result = prepare({
    config: { lynxtron: { runtimeVariant: 'release' } },
    args: ['--mas'],
    env: { LYNXTRON_RUNTIME_VARIANT: 'devtool' },
  });
  assert.equal(result.config.electronDownload.customFilename, 'lynxtron-v1.2.3-darwin-mas-arm64-devtool.zip');
});

test('explicit electronDownload remains authoritative while custom yml is stripped', () => {
  const electronDownload = { mirror: 'https://mirror.example.test/' };
  const result = prepare({ config: { lynxtron: { runtimeVariant: 'devtool' }, electronDownload } });
  assert.equal(result.config.electronDownload, electronDownload);
  assert.equal(result.config.lynxtron, undefined);
});

test('cross-platform targets select artifacts for the requested platform', () => {
  assert.equal(
    prepare({ args: ['--win', '--x64'] }).config.electronDownload.customFilename,
    'lynxtron-v1.2.3-win32-x64.zip'
  );
  assert.equal(
    prepare({ args: ['--linux', '--x64'] }).config.electronDownload.customFilename,
    'lynxtron-v1.2.3-linux-x64.zip'
  );
  assert.equal(
    prepare({ args: ['--mac', '--arm64'] }).config.electronDownload.customFilename,
    'lynxtron-v1.2.3-darwin-arm64.zip'
  );
});

test('macOS yml architecture selects the matching runtime artifact', () => {
  for (const [mac, expectedArch] of [
    [{ defaultArch: 'x64', target: 'dmg' }, 'x64'],
    [{ target: { target: 'dmg', arch: 'x64' } }, 'x64'],
    [{ target: [{ target: 'dmg', arch: ['arm64'] }] }, 'arm64'],
  ]) {
    const result = prepare({ config: { mac } });
    assert.equal(
      result.config.electronDownload.customFilename,
      `lynxtron-v1.2.3-darwin-${expectedArch}.zip`
    );
  }
});

test('CLI architecture overrides yml architecture', () => {
  const config = {
    mac: { target: [{ target: 'dmg', arch: ['universal'] }] },
  };
  const result = prepare({ config, args: ['--mac', '--arm64'] });
  assert.equal(
    result.config.electronDownload.customFilename,
    'lynxtron-v1.2.3-darwin-arm64.zip'
  );
  applyArchitectureToConfig(result.config, result.platform, result.arch);
  assert.equal(result.config.mac.target[0].arch, 'arm64');
});

test('configured architecture arrays are detected and filtered per build', () => {
  const config = {
    mac: {
      defaultArch: 'arm64',
      target: [
        { target: 'dmg', arch: ['x64', 'arm64'] },
        { target: 'zip', arch: 'arm64' },
      ],
    },
  };
  assert.deepEqual(resolveConfiguredArchitectures(config, 'darwin'), ['x64', 'arm64']);

  const x64Config = structuredClone(config);
  applyArchitectureToConfig(x64Config, 'darwin', 'x64', {
    filterTargets: true,
  });
  assert.deepEqual(x64Config.mac.target, [{ target: 'dmg', arch: 'x64' }]);

  const arm64Config = structuredClone(config);
  applyArchitectureToConfig(arm64Config, 'darwin', 'arm64', {
    filterTargets: true,
  });
  assert.deepEqual(arm64Config.mac.target, [
    { target: 'dmg', arch: 'arm64' },
    { target: 'zip', arch: 'arm64' },
  ]);
});

test('universal yml architecture accepts string and array forms', () => {
  for (const arch of ['universal', ['universal']]) {
    const config = { mac: { target: [{ target: 'dmg', arch }] } };
    assert.equal(isUniversalFromConfig(config), true);

    for (const slice of ['x64', 'arm64']) {
      const sliceConfig = structuredClone(config);
      applyArchitectureToConfig(sliceConfig, 'darwin', slice, {
        filterTargets: true,
      });
      assert.equal(sliceConfig.mac.target[0].arch, slice);
    }
  }
});

test('CLI architecture overrides a universal yml build plan', () => {
  const config = {
    mac: { target: [{ target: 'dmg', arch: ['universal'] }] },
  };
  assert.deepEqual(resolveBuildPlan(['--mac'], config, 'darwin'), {
    universal: true,
    architectures: ['x64', 'arm64'],
  });
  assert.deepEqual(resolveBuildPlan(['--mac', '--x64'], config, 'darwin'), {
    universal: false,
    architectures: ['x64'],
  });
  assert.deepEqual(
    resolveBuildPlan(['--win'], { ...config, win: { defaultArch: 'x64' } }, 'win32'),
    { universal: false, architectures: ['x64'] }
  );
});

test('universal slices select published macOS assets for both runtime variants', () => {
  for (const variant of ['release', 'devtool']) {
    const config = { lynxtron: { runtimeVariant: variant } };
    const suffix = variant === 'devtool' ? '-devtool' : '';
    assert.equal(
      prepare({ config: structuredClone(config), arch: '--x64' }).config.electronDownload.customFilename,
      `lynxtron-v1.2.3-darwin-x64${suffix}.zip`
    );
    assert.equal(
      prepare({ config: structuredClone(config), arch: '--arm64' }).config.electronDownload.customFilename,
      `lynxtron-v1.2.3-darwin-arm64${suffix}.zip`
    );
  }
});

test('universal packaging uses a sanitized config file', async () => {
  const debugLogger = { isEnabled: false, add() {} };
  const config = {
    productName: 'Example',
    directories: { output: 'dist' },
    mac: { target: [{ target: 'dmg', arch: 'universal' }] },
    lynxtron: { runtimeVariant: 'devtool' },
  };
  const result = prepareUniversalPackaging({
    config,
    configPath: '/project/config.json',
    outAppPath: '/project/dist/mac-universal/Example.app',
  });

  await assert.rejects(
    validateConfiguration(config, debugLogger),
    /unknown property 'lynxtron'/
  );
  await assert.doesNotReject(validateConfiguration(result.config, debugLogger));
  assert.equal(result.config.lynxtron, undefined);
  assert.deepEqual(result.args, [
    '-c',
    '/project/config.json',
    '--mac',
    '--prepackaged',
    '/project/dist/mac-universal/Example.app',
    '--universal',
  ]);
});
