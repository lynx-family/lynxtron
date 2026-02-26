/// <reference path="./tsconfig.tools.json" />

import { defineConfig } from '@rspack/cli';
import { rspack } from '@rspack/core';
import * as path from 'path';
import { fileURLToPath } from 'url';
import { pluginRspackDevReady } from '@lynx-js/dev-ready-plugin/rspack';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const isDev = process.env.NODE_ENV === 'development';

export default defineConfig({
  target: 'electron-main',
  entry: {
    main: './src/main/main.ts',
  },
  output: {
    path: path.resolve(__dirname, 'dist/assets/'),
    filename: '[name].js',
    module: true,
  },
  module: {
    rules: [
      {
        test: /\.svg$/,
        type: 'asset',
      },
    ],
  },
  plugins: [
    new rspack.CopyRspackPlugin({
      patterns: [
        { from: './package.json', to: 'package.json' },
        { from: './output/bundle/' },
      ],
    }),
    pluginRspackDevReady(),
  ],
  resolve: {
    extensions: ['.ts', '.js'],
  },
  optimization: {
    minimize: !isDev,
    nodeEnv: false,
  },
});
