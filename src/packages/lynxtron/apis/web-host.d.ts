// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

/** Browser main-thread handlers exposed through NativeModules.bridge. */
export interface MainThreadBridge {
  /** Receives the arguments passed by the Lynx background-thread caller. */
  [methodName: string]: (...args: any[]) => Promise<any> | any;
}

/** Adapter script loaded directly into the Lynx background Worker. */
export interface BackgroundServiceConfig {
  /**
   * URL of the built, importScripts-compatible adapter script.
   * Relative URLs are resolved against the browser page URL.
   * Export capabilities with contextBridge.exposeInLynxBTS; the UI accesses
   * them through NativeModules.nodejs.exposed.
   */
  scriptURL: string;
}

/** Optional browser host capabilities registered on a Lynx view. */
export interface HostConfig {
  /** Browser main-thread handlers called through RPC. */
  bridge?: MainThreadBridge;
  /** Background adapter whose exports become NativeModules.nodejs.exposed. */
  nodejs?: BackgroundServiceConfig;
}

/**
 * Configure browser host capabilities for a Lynx view.
 * Call from the browser main thread before loading the Lynx bundle.
 * Registers optional bridge handlers and a background adapter script.
 * This replaces the view's nativeModulesMap and onNativeModulesCall.
 * @see https://lynxjs.org/next/lynxtron/learn/running-on-web
 * @param lynxView The <lynx-view> element.
 * @param config Configuration for bridge and nodejs.
 */
export function setupSymmetricHost(lynxView: any, config?: HostConfig): void;
