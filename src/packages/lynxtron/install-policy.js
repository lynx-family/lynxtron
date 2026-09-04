export const POSTINSTALL_RUNTIME_VARIANT = 'devtool';

export function shouldSkipPostinstallRuntime(env = process.env) {
  return (
    env.LYNXTRON_SKIP_DOWNLOAD === '1' ||
    env.LYNXTRON_SKIP_DOWNLOAD === 'true'
  );
}

export function getPostinstallRuntimeOptions(env = process.env) {
  return {
    variant: POSTINSTALL_RUNTIME_VARIANT,
    customUrl:
      env.LYNXTRON_BINARY_URL ||
      env.npm_config_custom_lynxtron_binary_url,
    force: Boolean(env.npm_config_force_download),
  };
}
