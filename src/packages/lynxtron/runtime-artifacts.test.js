import test from 'node:test';
import assert from 'node:assert/strict';

import runtimeArtifacts from './runtime-artifacts.cjs';

const {
  DEFAULT_RUNTIME_DOWNLOAD_MIRROR,
  getRuntimeArtifactFilename,
  normalizeRuntimeVariant,
  parseRuntimeArguments,
  resolveRuntimeVariant,
} = runtimeArtifacts;

test('runtime downloads use the Lynxtron GitHub releases', () => {
  assert.equal(
    DEFAULT_RUNTIME_DOWNLOAD_MIRROR,
    'https://github.com/lynx-family/lynxtron/releases/download/'
  );
});

test('release and DevTool runtimes share a version and differ only by filename suffix', () => {
  assert.equal(
    getRuntimeArtifactFilename({ version: '1.2.3', platform: 'darwin', arch: 'arm64', variant: 'release' }),
    'lynxtron-v1.2.3-darwin-arm64.zip'
  );
  assert.equal(
    getRuntimeArtifactFilename({ version: '1.2.3', platform: 'darwin', arch: 'arm64', variant: 'devtool' }),
    'lynxtron-v1.2.3-darwin-arm64-devtool.zip'
  );
  assert.equal(
    getRuntimeArtifactFilename({ version: '1.2.3-beta.1', platform: 'win32', arch: 'x64', variant: 'devtool' }),
    'lynxtron-v1.2.3-beta.1-win32-x64-devtool.zip'
  );
});

test('MAS artifact naming remains compatible with electron-builder', () => {
  assert.equal(
    getRuntimeArtifactFilename({ version: '1.2.3', platform: 'darwin', arch: 'arm64', variant: 'release', mas: true }),
    'lynxtron-v1.2.3-darwin-mas-arm64.zip'
  );
});

test('runtime CLI options are consumed instead of forwarded to Lynxtron', () => {
  assert.deepEqual(
    parseRuntimeArguments(['--foo', '--lynxtron-runtime=release', 'app.js']),
    { cliVariant: 'release', forwardedArgs: ['--foo', 'app.js'] }
  );
  assert.deepEqual(
    parseRuntimeArguments(['--lynxtron-runtime', 'devtool', '--bar']),
    { cliVariant: 'devtool', forwardedArgs: ['--bar'] }
  );
});

test('runtime selection follows CLI, environment, config, default precedence', () => {
  assert.equal(
    resolveRuntimeVariant({ cliVariant: 'release', envVariant: 'devtool', configVariant: 'devtool', defaultVariant: 'devtool' }),
    'release'
  );
  assert.equal(
    resolveRuntimeVariant({ envVariant: 'release', configVariant: 'devtool', defaultVariant: 'devtool' }),
    'release'
  );
  assert.equal(
    resolveRuntimeVariant({ configVariant: 'devtool', defaultVariant: 'release' }),
    'devtool'
  );
});

test('invalid and missing runtime CLI values fail early', () => {
  assert.throws(() => normalizeRuntimeVariant('debug'), /Invalid Lynxtron runtime variant/);
  assert.throws(() => parseRuntimeArguments(['--lynxtron-runtime']), /requires a value/);
  assert.throws(() => parseRuntimeArguments(['--lynxtron-runtime=']), /Invalid Lynxtron runtime variant/);
  assert.throws(
    () => resolveRuntimeVariant({ envVariant: '', defaultVariant: 'devtool' }),
    /Invalid Lynxtron runtime variant/
  );
});
