const VALID_RUNTIME_VARIANTS = Object.freeze(['release', 'devtool']);
const DEFAULT_RUNTIME_DOWNLOAD_MIRROR =
  'https://github.com/lynx-family/lynxtron/releases/download/';

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

module.exports = {
  DEFAULT_RUNTIME_DOWNLOAD_MIRROR,
  VALID_RUNTIME_VARIANTS,
  getRuntimeArtifactFilename,
  normalizeRuntimeVariant,
  parseRuntimeArguments,
  resolveRuntimeVariant,
};
