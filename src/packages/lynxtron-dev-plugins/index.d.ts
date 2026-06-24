// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export declare function pluginLynxtron(options?: {
  isDev?: boolean;
  entry?: string;
  args?: string[];
  autolink?: boolean;
  env?: Record<string, string>;
  command?: string;
}): any;
