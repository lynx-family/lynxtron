const VALID_RUNTIME_VARIANTS = Object.freeze(['release', 'devtool']);

function normalizeRuntimeVariant(value, fallback = 'release') {
  const variant = value == null ? fallback : value;
  if (!VALID_RUNTIME_VARIANTS.includes(variant)) {
    throw new Error(
      `Invalid Lynxtron runtime variant "${variant}". Expected one of: ${VALID_RUNTIME_VARIANTS.join(', ')}`
    );
  }
  return variant;
}

function getRuntimeArtifactFilename({
  version,
  platform,
  arch,
  variant = 'release',
  mas = false,
}) {
  const normalizedVariant = normalizeRuntimeVariant(variant);
  const platformPart = mas ? `${platform}-mas` : platform;
  const variantPart = normalizedVariant === 'devtool' ? '-devtool' : '';
  return `lynxtron-v${version}-${platformPart}-${arch}${variantPart}.zip`;
}

function parseRuntimeArguments(args) {
  const forwardedArgs = [];
  let cliVariant;

  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === '--lynxtron-runtime') {
      if (index + 1 >= args.length) {
        throw new Error('--lynxtron-runtime requires a value');
      }
      cliVariant = normalizeRuntimeVariant(args[index + 1]);
      index += 1;
      continue;
    }
    if (arg.startsWith('--lynxtron-runtime=')) {
      cliVariant = normalizeRuntimeVariant(arg.slice('--lynxtron-runtime='.length));
      continue;
    }
    forwardedArgs.push(arg);
  }

  return { cliVariant, forwardedArgs };
}

function resolveRuntimeVariant({
  cliVariant,
  envVariant,
  configVariant,
  defaultVariant,
}) {
  const selectedVariant = [cliVariant, envVariant, configVariant].find(
    (value) => value != null
  );
  return normalizeRuntimeVariant(
    selectedVariant,
    defaultVariant
  );
}

const RUNTIME_PLATFORMS = Object.freeze([
  ['darwin', 'arm64'], ['darwin', 'x64'], ['linux', 'x64'], ['win32', 'x64'],
]);

function getRuntimePackageName({ platform, arch, variant = 'release' }) {
  if (!RUNTIME_PLATFORMS.some(([os, cpu]) => os === platform && cpu === arch)) {
    throw new Error(`Lynxtron npm runtime is not available for ${platform}-${arch}`);
  }
  const suffix = normalizeRuntimeVariant(variant) === 'devtool' ? '-devtool' : '';
  return `@lynx-js/lynxtron-runtime-${platform}-${arch}${suffix}`;
}

function getRuntimeExecutableRelativePath(platform) {
  if (platform === 'darwin') return 'lynxtron.app/Contents/MacOS/lynxtron';
  if (platform === 'win32') return 'lynxtron.exe';
  if (platform === 'linux') return 'lynxtron';
  throw new Error(`Unsupported runtime platform: ${platform}`);
}

module.exports = {
  RUNTIME_PLATFORMS,
  getRuntimePackageName,
  getRuntimeExecutableRelativePath,
  VALID_RUNTIME_VARIANTS,
  getRuntimeArtifactFilename,
  normalizeRuntimeVariant,
  parseRuntimeArguments,
  resolveRuntimeVariant,
};
