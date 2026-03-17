// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// --- Types ---
export interface APIS {
  [key: string]: unknown;
}

interface ContextBridge {
  exposeInLynxBTS(newApis: APIS): void;
}

declare global {
  var __contextBridge: ContextBridge;
}

const contextBridge = {
  exposeInLynxBTS: (newApis: APIS): void => {
    const fn = globalThis.__contextBridge?.exposeInLynxBTS;
    if (typeof fn !== 'function')
      throw new Error(
        'globalThis.__contextBridge.exposeInLynxBTS is not a function'
      );
    fn(newApis);
  },
};

export default contextBridge;
