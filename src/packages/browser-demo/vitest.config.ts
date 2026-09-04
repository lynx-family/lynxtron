import { defineConfig, mergeConfig } from 'vitest/config';
import { createVitestConfig } from '@lynx-js/react/testing-library/vitest-config';

const config = defineConfig({
  test: {},
});

export default async function createConfig() {
  const defaultConfig = await createVitestConfig();
  return mergeConfig(defaultConfig, config);
}
