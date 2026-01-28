import { defineConfig } from '@lynx-js/rspeedy'

import { pluginQRCode } from '@lynx-js/qrcode-rsbuild-plugin'
import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin'
import { pluginTypeCheck } from '@rsbuild/plugin-type-check'
import { pluginDevReady } from '@lynx-js/dev-ready-rsbuild-plugin'
import { fileURLToPath } from "url";
import path from "path";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootPath = process.cwd();

export default defineConfig({
  output: {
    assetPrefix: `file://${path.resolve(__dirname, "./dist/assets/")}/`,
    distPath: {
      root: './output/bundle',
    },
  },
  source: {
    entry: {
      main: './src/app/index.tsx',
    },
    alias: {
      "@assets": path.resolve(rootPath, "./src/assets"),
    }
  },
  plugins: [
    pluginQRCode({
      schema(url) {
        // We use `?fullscreen=true` to open the page in LynxExplorer in full screen mode
        return `${url}`
      },
    }),
    pluginReactLynx(),
    pluginTypeCheck(),
    pluginDevReady({ markerFile: path.resolve(__dirname, './output/bundle/dev-ready.json') }),
  ],
})
