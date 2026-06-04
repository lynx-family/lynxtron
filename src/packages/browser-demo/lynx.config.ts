import { defineConfig } from '@lynx-js/rspeedy';

import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin';
import { pluginRspeedyDevReady } from '@lynx-js/lynxtron-dev-plugins/rspeedy';
import { pluginTypeCheck } from '@rsbuild/plugin-type-check';
import { fileURLToPath, pathToFileURL } from 'url';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootPath = process.cwd();
console.log('rootPath: ', path.resolve(rootPath, './src/assets'));
export default defineConfig({
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
        alias: {
          '@assets': path.resolve(rootPath, './src/assets'),
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
        alias: {
          '@assets': path.resolve(rootPath, './src/assets'),
        },
      },
      output: {
        assetPrefix: new URL(
          './dist/desktop/',
          pathToFileURL(__dirname + path.sep)
        ).toString(),
        distPath: {
          root: './output/bundle/lynx',
        },
      },
    },
  },
  plugins: [
    pluginReactLynx(),
    pluginTypeCheck(),
    (() => {
      const p = pluginRspeedyDevReady({
        markerFile: path.resolve(__dirname, './output/bundle/dev-ready.json'),
      });
      // @ts-ignore
      delete p.apply;
      const originalSetup = p.setup;
      p.setup = (api: any) => {
        if (!api.onAfterDevCompile && api.onDevCompileDone) {
          api.onAfterDevCompile = api.onDevCompileDone;
        }
        return originalSetup(api);
      };
      return p;
    })(),
  ],
});
