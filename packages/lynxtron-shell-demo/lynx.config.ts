import { defineConfig } from '@lynx-js/rspeedy';

import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin';
import { pluginTypeCheck } from '@rsbuild/plugin-type-check';
import { pluginRspeedyDevReady } from '@lynx-js/lynxtron-dev-plugins/rspeedy';
import { fileURLToPath } from 'url';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootPath = process.cwd();
console.log('rootPath: ', path.resolve(rootPath, './src/assets'));
export default defineConfig({
  resolve: {
    alias: {
      '@assets': path.resolve(rootPath, './src/assets'),
    },
  },
  output: {
    filename: '[name].[platform].bundle',
  },
  environments: {
    /* WEB_SUPPORT_START */
    web: {
      source: {
        entry: {
          main: './src/app/index.tsx',
        },
      },
      output: {
        target: 'web',
        distPath: {
          root: './output/bundle/web',
        },
      },
    },
    /* WEB_SUPPORT_END */
    lynx: {
      source: {
        entry: {
          main: './src/app/index.tsx',
        },
      },
      output: {
        assetPrefix: `file://${path.resolve(__dirname, './dist/desktop/')}/`,
        distPath: {
          root: './output/bundle/lynx',
        },
      },
    },
  },
  plugins: [pluginReactLynx(), pluginTypeCheck(), pluginRspeedyDevReady()],
});
