import { defineConfig } from '@lynx-js/rspeedy';
import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin';

export default defineConfig({
  output: {
    filename: {
      bundle: 'headless_contenteditable_text.bundle',
    },
  },
  source: {
    entry: {
      main: './index.tsx',
    },
  },
  plugins: [pluginReactLynx()],
});
