// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export interface MainThreadBridge {
  [methodName: string]: (data: any) => Promise<any> | any;
}

export interface BackgroundServiceConfig {
  workerURL: string;
  methods: string[];
}

export interface HostConfig {
  bridge?: MainThreadBridge;
  nodejs?: BackgroundServiceConfig;
}

/**
 * Setup Symmetric Host for Web.
 * @param lynxView The <lynx-view> element.
 * @param config Configuration for bridge and nodejs.
 */
export function setupSymmetricHost(lynxView: any, config?: HostConfig): void;
