import { defineConfig } from '@lynx-js/rspeedy';
import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin';

export default defineConfig({
  output: {
    filename: {
      bundle: 'lynx_open_interface_e2e.bundle',
    },
  },
  source: {
    entry: {
      main: './src/app.tsx',
    },
  },
  plugins: [pluginReactLynx()],
});
