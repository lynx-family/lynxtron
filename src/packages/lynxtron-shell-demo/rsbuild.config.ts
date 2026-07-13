// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

/// <reference path="./tsconfig.tools.json" />

import { defineConfig } from '@rsbuild/core';
import { pluginLynxtron } from '@lynx-js/lynxtron-dev-plugins/rsbuild';
/* WEB_SUPPORT_START */
const rspeedyDevServer = 'http://localhost:5969';
/* WEB_SUPPORT_END */

export default defineConfig({
  /* WEB_SUPPORT_START */
  server: {
    port: 8080,
    historyApiFallback: true,
    proxy: [
      {
        pathFilter: (pathname: string) =>
          pathname.endsWith('.bundle') ||
          pathname.endsWith('.map') ||
          pathname.includes('__rspeedy') ||
          pathname.includes('/static/'),
        target: rspeedyDevServer,
        pathRewrite: {
          '^/web/': '/',
        },
      },
    ],
  },
  /* WEB_SUPPORT_END */
  environments: {
    desktop: {
      source: {
        entry: {
          main: './src/main/desktop/main.ts',
          preload: './src/main/desktop/preload.ts',
        },
      },
      plugins: [
        pluginLynxtron({
          args: ['--inspect=9222'],
        }),
      ],
      output: {
        target: 'node',
        distPath: {
          root: './dist/desktop',
        },
        copy: [
          { from: './package.json', to: 'package.json' },
          { from: './output/bundle/lynx/', to: '.' },
        ],
      },
      dev: {
        writeToDisk: true,
      },
    },
    /* WEB_SUPPORT_START */
    web: {
      source: {
        entry: {
          'web-host': './src/main/web/web-host.ts',
          'nodejs-adapter-web': {
            import: './src/main/web/nodejs_adapter_web.ts',
            html: false,
          },
        },
      },
      output: {
        target: 'web',
        // Derive async chunk and asset URLs from the current script so the
        // output can be served from either / or a nested public path.
        assetPrefix: 'auto',
        filenameHash: false,
        filename: {
          html: 'index.html',
        },
        distPath: {
          root: './dist/web',
          js: '',
          jsAsync: '',
        },
        copy: [{ from: './output/bundle/web/', to: '.' }],
      },
      html: {
        template: './src/main/web/index.html',
        inject: 'body',
      },
      splitChunks: false,
    },
    /* WEB_SUPPORT_END */
  },
});
