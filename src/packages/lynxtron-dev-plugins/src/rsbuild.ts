// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import path from 'node:path';
import { readFileSync } from 'node:fs';
import type { RsbuildPlugin, Rspack } from '@rsbuild/core';
import { pluginLynxtron as pluginLynxtronRspack } from './rspack.js';
import type { PluginLynxtronRspackOptions } from './rspack.js';

const DEFAULT_WATCH_IGNORED = [
  '**/node_modules/.cache/**',
  '**/src/app/**',
  '**/output/bundle/lynx/**',
];

export interface PluginLynxtronOptions
  extends Omit<PluginLynxtronRspackOptions, 'isDev' | 'entry'> {
  /**
   * Directory passed to the Lynxtron runtime in development.
   * Defaults to the current Rsbuild environment's output directory.
   */
  entry?: string;
  /**
   * Additional Rspack watch options. Lynxtron UI and Rspeedy output paths are
   * ignored by default so UI rebuilds do not restart the Desktop Host.
   */
  watchOptions?: Rspack.Configuration['watchOptions'];
}

export function pluginLynxtron(
  options: PluginLynxtronOptions = {}
): RsbuildPlugin {
  const { entry, watchOptions, ...rspackPluginOptions } = options;

  return {
    name: 'lynxtron-rsbuild-plugin',
    setup(api) {
      api.modifyRspackConfig({
        order: 'post',
        handler(config, { environment, isDev }) {
          if (environment.config.output.target !== 'node') {
            throw new Error(
              `pluginLynxtron requires output.target to be "node" in the "${environment.name}" environment.`
            );
          }

          config.target = 'electron-main';
          // Production dependencies are shipped by the app packager. Keep their
          // package-relative assets and native addon loaders intact at runtime.
          const manifest = JSON.parse(
            readFileSync(
              path.join(api.context.rootPath, 'package.json'),
              'utf8'
            )
          );
          const dependencies = new Set([
            ...Object.keys(manifest.dependencies ?? {}),
            ...Object.keys(manifest.optionalDependencies ?? {}),
          ]);
          const existing = config.externals;
          config.externals = [
            ...(Array.isArray(existing)
              ? existing
              : existing
              ? [existing]
              : []),
            ({ request }, callback) => {
              const packageName = request?.startsWith('@')
                ? request.split('/').slice(0, 2).join('/')
                : request?.split('/')[0];
              if (dependencies.has(packageName)) {
                callback(null, `node-commonjs ${request}`);
              } else {
                callback();
              }
            },
          ];
          config.node = {
            ...(typeof config.node === 'object' ? config.node : {}),
            __dirname: 'node-module',
            __filename: 'node-module',
          };
          config.watchOptions = {
            ...config.watchOptions,
            ...watchOptions,
            ignored:
              watchOptions?.ignored ??
              config.watchOptions?.ignored ??
              DEFAULT_WATCH_IGNORED,
          };
          config.plugins.push(
            pluginLynxtronRspack({
              ...rspackPluginOptions,
              isDev,
              entry:
                entry === undefined
                  ? environment.distPath
                  : path.resolve(api.context.rootPath, entry),
            })
          );
        },
      });
    },
  };
}
