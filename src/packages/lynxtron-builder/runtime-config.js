const ARCH_FLAGS = new Map([
  ['--x64', 'x64'],
  ['--arm64', 'arm64'],
  ['--ia32', 'ia32'],
  ['--armv7l', 'armv7l'],
]);

function normalizeArchitectures(value) {
  const values = Array.isArray(value) ? value : [value];
  return values
    .filter(item => typeof item === 'string' && item.length > 0)
    .map(item => item.replace(/^--/, ''));
}

function resolvePlatformConfig(config, platform) {
  const key = platform === 'darwin'
    ? 'mac'
    : platform === 'win32'
      ? 'win'
      : platform;
  return config && config[key];
}

function resolveCliArchitectures(args) {
  return [...new Set(args.map(arg => ARCH_FLAGS.get(arg)).filter(Boolean))];
}

function resolveConfiguredArchitectures(config, platform) {
  const platformConfig = resolvePlatformConfig(config, platform);
  if (!platformConfig || typeof platformConfig !== 'object') {
    return [];
  }

  const defaultArchitectures = [
    ...normalizeArchitectures(platformConfig.defaultArch),
    // Retain compatibility with the legacy field consumed by the existing
    // universal build path, even though electron-builder calls it defaultArch.
    ...normalizeArchitectures(platformConfig.arch),
  ];
  const targetArchitectures = [];
  const targets = Array.isArray(platformConfig.target)
    ? platformConfig.target
    : [platformConfig.target];
  for (const target of targets) {
    if (target === 'universal') {
      targetArchitectures.push('universal');
    } else if (target && typeof target === 'object') {
      targetArchitectures.push(...normalizeArchitectures(target.arch));
    }
  }
  return [...new Set(
    targetArchitectures.length > 0
      ? targetArchitectures
      : defaultArchitectures
  )];
}

function resolveArchitecture(args, arch, configuredArchitectures, defaultArch) {
  if (arch) {
    return normalizeArchitectures(arch)[0];
  }
  const cliArchitectures = resolveCliArchitectures(args);
  if (cliArchitectures.length > 0) return cliArchitectures[0];
  if (configuredArchitectures.length === 1) return configuredArchitectures[0];
  return defaultArch;
}

function resolveTargetPlatform(args, defaultPlatform) {
  if (args.includes('--mac') || args.includes('-m') || args.includes('--mas')) {
    return 'darwin';
  }
  if (args.includes('--win') || args.includes('-w')) {
    return 'win32';
  }
  if (args.includes('--linux') || args.includes('-l')) {
    return 'linux';
  }
  return defaultPlatform;
}

function sanitizeBuilderConfig(config) {
  const sanitizedConfig = { ...config };
  delete sanitizedConfig.lynxtron;
  return sanitizedConfig;
}

function isUniversalFromConfig(config) {
  return resolveConfiguredArchitectures(config, 'darwin').includes('universal');
}

function resolveBuildPlan(args, config, platform) {
  const cliArchitectures = resolveCliArchitectures(args);
  const configuredArchitectures = resolveConfiguredArchitectures(
    config,
    platform
  );
  const universal = args.includes('--universal') ||
    (platform === 'darwin' &&
      cliArchitectures.length === 0 &&
      isUniversalFromConfig(config));
  if (universal) {
    return { universal: true, architectures: ['x64', 'arm64'] };
  }
  return {
    universal: false,
    architectures: cliArchitectures.length > 0
      ? cliArchitectures
      : configuredArchitectures.filter(arch => arch !== 'universal'),
  };
}

function applyArchitectureToConfig(
  config,
  platform,
  arch,
  { filterTargets = false } = {}
) {
  const platformConfig = resolvePlatformConfig(config, platform);
  if (!platformConfig || typeof platformConfig !== 'object') {
    return config;
  }

  if (platformConfig.defaultArch) {
    platformConfig.defaultArch = arch;
  }
  if (platformConfig.arch) {
    platformConfig.arch = arch;
  }

  if (platformConfig.target == null) {
    return config;
  }
  const wasArray = Array.isArray(platformConfig.target);
  const targets = wasArray ? platformConfig.target : [platformConfig.target];
  const rewritten = targets.flatMap(target => {
    if (target === 'universal') {
      return [];
    }
    if (!target || typeof target !== 'object') {
      return [target];
    }
    const targetArchitectures = normalizeArchitectures(target.arch);
    if (
      filterTargets &&
      targetArchitectures.length > 0 &&
      !targetArchitectures.includes(arch) &&
      !targetArchitectures.includes('universal')
    ) {
      return [];
    }
    return targetArchitectures.length > 0 ? [{ ...target, arch }] : [target];
  });

  if (rewritten.length === 0) {
    delete platformConfig.target;
  } else {
    platformConfig.target = wasArray ? rewritten : rewritten[0];
  }
  return config;
}

function prepareUniversalPackaging({ config, configPath, outAppPath }) {
  return {
    config: sanitizeBuilderConfig(config),
    args: ['-c', configPath, '--mac', '--prepackaged', outAppPath, '--universal'],
  };
}

function prepareRuntimeConfig({
  config,
  args,
  env,
  arch,
  platform,
  defaultArch,
  lynxtronVersion,
  runtimeArtifacts,
}) {
  const { cliVariant, forwardedArgs } = runtimeArtifacts.parseRuntimeArguments(args);
  const configVariant = config.lynxtron && config.lynxtron.runtimeVariant;
  const variant = runtimeArtifacts.resolveRuntimeVariant({
    cliVariant,
    envVariant: env.LYNXTRON_RUNTIME_VARIANT,
    configVariant,
    defaultVariant: 'release',
  });
  const resolvedPlatform = resolveTargetPlatform(forwardedArgs, platform);
  const configuredArchitectures = resolveConfiguredArchitectures(
    config,
    resolvedPlatform
  );
  const resolvedArch = resolveArchitecture(
    forwardedArgs,
    arch,
    configuredArchitectures,
    defaultArch
  );

  delete config.lynxtron;

  if (!config.electronDownload) {
    const effectiveVersion = config.electronVersion || lynxtronVersion;
    if (!config.electronVersion) {
      config.electronVersion = effectiveVersion;
    }
    config.electronDownload = {
      version: effectiveVersion,
      mirror: runtimeArtifacts.DEFAULT_RUNTIME_DOWNLOAD_MIRROR,
      customDir: `v${effectiveVersion}`,
      customFilename: runtimeArtifacts.getRuntimeArtifactFilename({
        version: effectiveVersion,
        platform: resolvedPlatform,
        arch: resolvedArch,
        variant,
        mas: forwardedArgs.includes('--mas'),
      }),
    };
  }

  return {
    config,
    forwardedArgs,
    variant,
    arch: resolvedArch,
    platform: resolvedPlatform,
  };
}

module.exports = {
  applyArchitectureToConfig,
  isUniversalFromConfig,
  prepareRuntimeConfig,
  prepareUniversalPackaging,
  resolveArchitecture,
  resolveBuildPlan,
  resolveCliArchitectures,
  resolveConfiguredArchitectures,
  resolveTargetPlatform,
  sanitizeBuilderConfig,
};
