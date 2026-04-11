// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { EventEmitter } from 'events';
import { Readable } from 'stream';

// UtilityProcess API

export class UtilityProcess extends EventEmitter {
  /**
   * The `PID` of the process.
   */
  readonly pid: number | undefined;
  /**
   * A `Readable Stream` that represents the process's stdout.
   */
  readonly stdout: Readable | null;
  /**
   * A `Readable Stream` that represents the process's stderr.
   */
  readonly stderr: Readable | null;
  /**
   * Terminates the process gracefully. On some operating systems, the process will be
   * terminated immediately.
   */
  kill(): boolean;
  /**
   * Send a message to the child process.
   */
  postMessage(message: any, transfer?: any[]): void;
}

export namespace utilityProcess {
  /**
   * Spawns a new process that runs the specified `modulePath`.
   */
  function fork(
    modulePath: string,
    args?: string[],
    options?: ForkOptions
  ): UtilityProcess;
}

export interface ForkOptions {
  /**
   * Environment key-value pairs. Default is `process.env`.
   */
  env?: Record<string, string>;
  /**
   * List of string arguments passed to the executable.
   */
  execArgv?: string[];
  /**
   * Current working directory of the child process.
   */
  cwd?: string;
  /**
   * If `true`, the child's stdin, stdout, and stderr will be piped to the parent,
   * otherwise they will be inherited from the parent. Default is `false`.
   */
  stdio?: 'pipe' | 'ignore' | 'inherit' | ('pipe' | 'ignore' | 'inherit')[];
  /**
   * Allows configuring the mode for `stdout` and `stderr` of the child process.
   * Default is `inherit`. String value can be one of `pipe`, `ignore`, `inherit`.
   */
  serviceName?: string;
  /**
   * With this flag, the utility process will be launched via the `allow_child_plugin`
   * mechanism of the `UtilityProcess` API.
   */
  allowLoadingUnsignedLibraries?: boolean;
}
