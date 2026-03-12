import { defineConfig } from '@lynx-js/rspeedy';
import { pluginReactLynx } from '@lynx-js/react-rsbuild-plugin';
import { pluginTypeCheck } from '@rsbuild/plugin-type-check';
import path from 'node:path';

import cardsConfig from './cards.config.js';

const { entries } = cardsConfig as { entries: Record<string, string> };
const defaultCard = Object.keys(entries)[0] ?? 'bridging-lynx-node';
const selectedCard =
  (process.env.LYNX_CARD as string | undefined) ?? defaultCard;
const entry = entries[selectedCard] ?? entries[defaultCard];

export default defineConfig({
  output: {
    minify: false,
    cleanDistPath: false,
    filename: `${selectedCard}.lynx.bundle`,
  },
  environments: {
    lynx: {
      source: {
        entry: {
          main: entry,
        },
      },
      output: {
        assetPrefix: `file://${path.resolve(process.cwd(), './dist')}/`,
        distPath: {
          root: './dist',
        },
      },
    },
  },
  plugins: [pluginReactLynx(), pluginTypeCheck()],
});
