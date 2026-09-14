// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { execSync } from 'child_process';
import spawn from 'cross-spawn';
import { applyLynxtronAutoLink } from './autolink-rspack.js';

const isWin = process.platform === 'win32';

export interface PluginLynxtronRspackOptions {
  isDev?: boolean;
  entry?: string;
  args?: string[];
  autolink?: boolean;
  env?: Record<string, string>;
  command?: string;
}

export function pluginLynxtron(options: PluginLynxtronRspackOptions = {}): any {
  const {
    isDev,
    entry,
    args = [],
    autolink = true,
    env,
    command = 'lynxtron',
  } = options;
  return {
    name: 'lynxtron-plugin',
    apply(compiler: any) {
      let lynxtronProcess: ReturnType<typeof spawn> | null = null;
      let timeout: NodeJS.Timeout;
      let closed = false;
      const stop = () => {
        clearTimeout(timeout);
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
        lynxtronProcess = null;
      };
      const close = () => {
        closed = true;
        stop();
        process.removeListener('exit', close);
        process.removeListener('SIGINT', onSignal);
        process.removeListener('SIGTERM', onSignal);
      };
      const onSignal = (signal: NodeJS.Signals) => {
        close();
        // Let the build tool's own signal handlers finish their cleanup. If it
        // has none, restore the signal's default termination behavior.
        if (process.listenerCount(signal) === 0)
          process.kill(process.pid, signal);
      };
      if (isDev && entry) {
        process.once('exit', close);
        process.once('SIGINT', onSignal);
        process.once('SIGTERM', onSignal);
        compiler.hooks.watchClose?.tap('LynxtronStop', close);
        compiler.hooks.shutdown?.tap('LynxtronStop', close);
      }
      const restart = () => {
        if (closed) return;
        stop();
        timeout = setTimeout(() => {
          if (closed) return;
          // The Lynxtron runtime treats argv[1] as the application directory.
          // Keep it first so runtime flags cannot be mistaken for the app path by
          // the runtime or generated AutoLink loaders.
          const spawnArgs = [entry, ...args];

          const child = (lynxtronProcess = spawn(command, spawnArgs, {
            stdio: 'inherit',
            env: {
              ...process.env,
              ...env,
            },
            detached: true,
          }));
          child.once('exit', () => {
            if (lynxtronProcess === child) lynxtronProcess = null;
          });
          child.once('error', (error) => process.emitWarning(error));
        }, 300);
      };
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
        restart();
      });
    },
  };
}
