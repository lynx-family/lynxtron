// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

/// <reference path="./tsconfig.tools.json" />

import { defineConfig } from '@rsbuild/core';
import { pluginLynxtron } from '@lynx-js/lynxtron-dev-plugins/rsbuild';

export default defineConfig({
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
  },
});
