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
              })
            );
          } catch {}
        }
      });
    },
  };
}
