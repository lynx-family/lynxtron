// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export interface Devtool {
  /**
   * Set whether to enable the Lynx devtool.
   */
  setDevToolEnabled(enabled: boolean): void;
  /**
   * Get whether the Lynx devtool is enabled.
   */
  isDevtoolEnabled(): boolean;
  /**
   * Set whether to enable the Lynx logbox.
   */
  setLogboxEnabled(enable: boolean): void;
  /**
   * Get whether the Lynx logbox is enabled.
   */
  isLogboxEnabled(): boolean;
  /**
   * Set the callback for handling open-card requests from the Lynx devtool.
   * Pass null to clear the current callback.
   */
  setOpenCardCallback(callback: ((url: string) => void) | null): void;

  /**
   * Connect to the Lynx devtool.
   */
  connectDevtool(url: string): void;
}

export declare const devtool: Devtool;
