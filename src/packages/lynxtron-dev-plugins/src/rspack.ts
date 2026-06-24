// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { spawn, execSync } from 'child_process';
import { applyLynxtronAutoLink } from './autolink-rspack.js';

let lynxtronProcess: ReturnType<typeof spawn> | null = null;
let queue = Promise.resolve<ReturnType<typeof spawn> | null>(null);
const isWin = process.platform === 'win32';

function debounce(func: (...args: any[]) => void, wait: number) {
  let timeout: NodeJS.Timeout;
  return function (this: any, ...args: any[]) {
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(this, args), wait);
  };
}

const restartLynxtron = debounce(
  (options: {
    entry: string;
    args?: string[];
    env?: Record<string, string>;
    command?: string;
  }) => {
    queue = queue.then(() => {
      try {
        if (lynxtronProcess && lynxtronProcess.pid) {
          const pid = lynxtronProcess.pid;
          if (isWin) {
            execSync(`taskkill /pid ${pid} /f /t`);
          } else {
            try {
              process.kill(-pid, 'SIGKILL');
            } catch {}
          }
        }
      } catch {}

      const { entry, args = [], env = {}, command = 'lynxtron' } = options;
      const spawnArgs = [...args, entry];

      lynxtronProcess = spawn(command, spawnArgs, {
        stdio: 'inherit',
        shell: true,
        env: {
          ...process.env,
          ...env,
        },
        detached: true,
      });

      return lynxtronProcess;
    });
  },
  300
);

export function pluginLynxtron(
  options: {
    isDev?: boolean;
    entry?: string;
    args?: string[];
    autolink?: boolean;
    env?: Record<string, string>;
    command?: string;
  } = {}
): any {
  const { isDev, entry, args = [], autolink = true, env, command } = options;
  return {
    name: 'lynxtron-plugin',
    apply(compiler: any) {
      if (autolink) {
        applyLynxtronAutoLink(compiler);
      }

      if (!compiler.options.optimization) {
        compiler.options.optimization = {};
      }
      compiler.options.optimization.minimize = !isDev;
      compiler.options.optimization.nodeEnv = false;

      compiler.hooks.done.tap('LynxtronStart', () => {
        if (!isDev || !entry) {
          return;
        }
        const extraArgs = [...args];
        restartLynxtron({
          entry,
          args: extraArgs,
          env,
          command,
        });
      });
    },
  };
}
