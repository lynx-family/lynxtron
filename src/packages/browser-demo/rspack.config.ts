/// <reference path="./tsconfig.tools.json" />

import { defineConfig } from '@rspack/cli';
import { rspack } from '@rspack/core';
import * as path from 'path';
import { fileURLToPath } from 'url';
import { pluginLynxtron } from '@lynx-js/lynxtron-dev-plugins/rspack';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const isDev = process.env.NODE_ENV === 'development';

const desktopConfig = defineConfig({
  target: 'electron-main',
  entry: {
    main: './src/main/desktop/main.ts',
    preload: './src/main/desktop/preload.ts',
  },
  output: {
    path: path.resolve(__dirname, 'dist/desktop/'),
    filename: '[name].js',
  },
  devServer: {
    devMiddleware: {
      writeToDisk: true,
    },
  },
  watchOptions: {
    ignored: ['**/src/app/**', '**/output/bundle/lynx/**'],
  },
  module: {
    rules: [
      {
        test: /\.ts$/,
        exclude: [/node_modules/],
        loader: 'builtin:swc-loader',
        options: {
          jsc: {
            parser: {
              syntax: 'typescript',
            },
          },
        },
        type: 'javascript/auto',
      },
      {
        test: /\.svg$/,
        type: 'asset',
      },
    ],
  },
  plugins: [
    pluginLynxtron({
      isDev,
      entry: path.resolve(__dirname, './dist/desktop'),
      args: isDev ? ['--inspect=9222'] : [],
    }),
    new rspack.CopyRspackPlugin({
      patterns: [
        { from: './package.json', to: 'package.json' },
        { from: './output/bundle/lynx/', to: '.' },
      ],
    }),
  ],
  externalsPresets: { node: true },
  externals: {
    '@lynx-js/cef-webview': 'commonjs @lynx-js/cef-webview',
  },
  resolve: {
    extensions: ['.ts', '.js'],
  },
});

/* WEB_SUPPORT_START */
const webConfig = defineConfig({
  target: 'web',
  entry: {
    'web-host': './src/main/web/web-host.ts',
    'nodejs-adapter-web': './src/main/web/nodejs_adapter_web.ts',
  },
  output: {
    path: path.resolve(__dirname, 'dist/web/'),
    filename: '[name].js',
  },
  experiments: {
    asyncWebAssembly: true,
    css: true,
  },
  module: {
    rules: [
      {
        test: /\.ts$/,
        exclude: [/node_modules/],
        loader: 'builtin:swc-loader',
        options: {
          jsc: {
            parser: {
              syntax: 'typescript',
            },
          },
        },
        type: 'javascript/auto',
      },
      {
        test: /\.css$/,
        type: 'css',
      },
      {
        test: /\.(png|svg|jpg)$/,
        type: 'asset',
      },
    ],
  },
  plugins: [
    new rspack.HtmlRspackPlugin({
      template: './src/main/web/index.html',
      filename: 'index.html',
      chunks: ['web-host'],
    }),
    new rspack.CopyRspackPlugin({
      patterns: [{ from: './output/bundle/web/', to: '.' }],
    }),
  ],
  resolve: {
    extensions: ['.ts', '.js', '.tsx'],
  },
  optimization: {
    minimize: !isDev,
  },
  devServer: {
    port: 8080,
    historyApiFallback: true,
    proxy: [
      {
        context: (pathname: string) => {
          return (
            pathname.endsWith('.bundle') ||
            pathname.endsWith('.map') ||
            pathname.includes('__rspeedy') ||
            pathname.includes('/static/')
          );
        },
        target: 'http://localhost:3000',
        pathRewrite: {
          '^/web/': '/',
        },
      },
    ],
  },
});
/* WEB_SUPPORT_END */

const targets = (process.env.TARGET_ENV || 'desktop,web').split(',');

const configs = [];
if (targets.includes('desktop')) {
  configs.push(desktopConfig);
}
if (targets.includes('web')) {
  /* WEB_SUPPORT_START */
  configs.push(webConfig);
  /* WEB_SUPPORT_END */
}

export default configs;
