import { defineConfig } from '@lynx-js/rspeedy';

import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin';
import { pluginTypeCheck } from '@rsbuild/plugin-type-check';
import { pluginRspeedyDevReady } from '@lynx-js/dev-ready-plugin/rspeedy';
import { fileURLToPath } from 'url';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootPath = process.cwd();

export default defineConfig({
  output: {
    filename: {
      bundle: 'main.lynx.bundle',
    },
    assetPrefix: `file://${path.resolve(__dirname, './dist/assets/')}/`,
    distPath: {
      root: './output/bundle',
    },
  },
  source: {
    entry: {
      main: './src/app/index.tsx',
    },
    alias: {
      '@assets': path.resolve(rootPath, './src/assets'),
    },
  },
  plugins: [pluginReactLynx(), pluginTypeCheck(), pluginRspeedyDevReady()],
});
