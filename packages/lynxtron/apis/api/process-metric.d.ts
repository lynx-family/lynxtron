// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export interface CPUUsage {
  // Docs: https://electronjs.org/docs/api/structures/cpu-usage

  /**
   * Total seconds of CPU time used since process startup.
   */
  cumulativeCPUUsage?: number;
  /**
   * The number of average idle CPU wakeups per second since the last call to
   * getCPUUsage. First call returns 0. Will always return 0 on Windows.
   */
  idleWakeupsPerSecond: number;
  /**
   * Percentage of CPU used since the last call to getCPUUsage. First call returns 0.
   */
  percentCPUUsage: number;
}

export interface MemoryInfo {
  // Docs: https://electronjs.org/docs/api/structures/memory-info

  /**
   * The maximum amount of memory that has ever been pinned to actual physical RAM.
   */
  peakWorkingSetSize: number;
  /**
   * The amount of memory not shared by other processes, such as JS heap or HTML
   * content.
   *
   * @platform win32
   */
  privateBytes?: number;
  /**
   * The amount of memory currently pinned to actual physical RAM.
   */
  workingSetSize: number;
}

export interface ProcessMetric {
  // Docs: https://electronjs.org/docs/api/structures/process-metric

  /**
   * CPU usage of the process.
   */
  cpu: CPUUsage;
  /**
   * Creation time for this process. The time is represented as number of
   * milliseconds since epoch. Since the `pid` can be reused after a process dies, it
   * is useful to use both the `pid` and the `creationTime` to uniquely identify a
   * process.
   */
  creationTime: number;
  /**
   * One of the following values:
   *
   * @platform win32
   */
  integrityLevel?: 'untrusted' | 'low' | 'medium' | 'high' | 'unknown';
  /**
   * Memory information for the process.
   */
  memory: MemoryInfo;
  /**
   * The name of the process. Examples for utility: `Audio Service`, `Content
   * Decryption Module Service`, `Network Service`, `Video Capture`, etc.
   */
  name?: string;
  /**
   * Process id of the process.
   */
  pid: number;
  /**
   * Whether the process is sandboxed on OS level.
   *
   * @platform darwin,win32
   */
  sandboxed?: boolean;
  /**
   * The non-localized name of the process.
   */
  serviceName?: string;
  /**
   * Process type. One of the following values:
   */
  type:
    | 'Browser'
    | 'Tab'
    | 'Utility'
    | 'Zygote'
    | 'Sandbox helper'
    | 'GPU'
    | 'Pepper Plugin'
    | 'Pepper Plugin Broker'
    | 'Unknown';
}
