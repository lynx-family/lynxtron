// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import fs from 'fs';
import path from 'path';

export function pluginRspeedyDevReady() {
  const readyLine = 'RSPEEDY_READY';
  const serverPrefix = 'RSPEEDY_DEV_SERVER:';
  return {
    name: 'dev-ready-rspeedy-plugin',
    apply: 'serve',
    setup(api: any) {
      api.modifyRsbuildConfig({
        order: 'post',
        handler(config: any) {
          const originalPrintUrls =
            api.getRsbuildConfig('original').server?.printUrls;
          const printUrls = config.server?.printUrls;
          if (
            originalPrintUrls === false ||
            typeof originalPrintUrls === 'function' ||
            typeof printUrls !== 'function'
          ) {
            return config;
          }
          return {
            ...config,
            server: {
              ...config.server,
              printUrls(params: any) {
                const environments = api.getNormalizedConfig().environments;
                if (
                  Object.keys(config.environments ?? {}).every(
                    (name) => name in environments,
                  )
                ) {
                  return printUrls(params);
                }

                // Rspeedy's URL printer captures all configured environments,
                // including ones excluded by --environment. Read only the
                // active normalized configurations when printing instead.
                const rspeedy = api.useExposed(Symbol.for('rspeedy.api'));
                if (!rspeedy) return printUrls(params);
                const filename = rspeedy.config.output?.filename;
                const bundle =
                  typeof filename === 'string'
                    ? filename
                    : (filename?.bundle ??
                      filename?.template ??
                      '[name].[platform].bundle');
                return Object.entries(environments).flatMap(
                  ([name, environment]: [string, any]) => {
                    const host = environment.dev.client?.host ?? 'localhost';
                    const prefix = environment.dev.assetPrefix;
                    const base = (
                      typeof prefix === 'string'
                        ? prefix
                        : `http://${host}:<port>/`
                    ).replaceAll('<port>', String(params.port));
                    return Object.keys(environment.source.entry).flatMap(
                      (entryName) => {
                        const template =
                          typeof bundle === 'function'
                            ? bundle({
                                entryName,
                                platform: name,
                                lazyBundle: false,
                              })
                            : bundle;
                        const pathname = template
                          .replaceAll('[name]', entryName)
                          .replaceAll('[platform]', name);
                        const urls = [
                          {
                            label: name.charAt(0).toUpperCase() + name.slice(1),
                            url: new URL(pathname, base).toString(),
                          },
                        ];
                        if (name === 'web') {
                          urls.push({
                            label: '∟ Preview',
                            url: new URL(
                              `/__web_preview?casename=${encodeURIComponent(pathname)}`,
                              base,
                            ).toString(),
                          });
                        }
                        return urls;
                      },
                    );
                  },
                );
              },
            },
          };
        },
      });
      let done = false;
      api.onAfterStartDevServer((params: any) => {
        const port = params && params.port;
        if (port) process.stdout.write(`${serverPrefix}${port}\n`);
      });
      api.onAfterDevCompile((params: any) => {
        const first = params && params.isFirstCompile;
        if (!done && first) {
          done = true;
          process.stdout.write(`${readyLine}\n`);
          const dist = path.join(process.cwd(), './.tmp');
          const file = path.join(dist, 'dev-ready.rspeedy.json');
          try {
            fs.mkdirSync(path.dirname(file), { recursive: true });
            fs.writeFileSync(
              file,
              JSON.stringify({
                ready: true,
                source: 'rspeedy',
                time: Date.now(),
              }),
            );
          } catch {}
        }
      });
    },
  };
}
