// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import runPreloadScripts from './api/preload-runner';
import { APIS } from './api/context-bridge';

// --- Context Bridge Setup ---
(() => {
  let apisTarget: APIS | null = null;
  const apis: APIS = {};

  function mergeInPlace(target: APIS, source: APIS) {
    for (const [k, v] of Object.entries(source || {})) {
      target[k] = v;
    }
  }

  function initModuleAPI(moduleApis: APIS) {
    apisTarget = moduleApis;
    mergeInPlace(apisTarget, apis);
  }

  function exposeInLynxBTS(newApis: APIS) {
    if (!newApis || typeof newApis !== 'object') {
      return;
    }

    mergeInPlace(apis, newApis);
    if (apisTarget) {
      mergeInPlace(apisTarget, newApis);
    }
  }

  Object.defineProperty(globalThis, '__contextBridge', {
    value: Object.freeze({
      initModuleAPI,
      exposeInLynxBTS,
    }),
    enumerable: false,
    configurable: false,
    writable: false,
  });
})();

const Module = require('module') as NodeJS.ModuleInternal;

const makeLynxtronModule = (name: string) => {
  const lynxtronModule = new Module('lynxtron', null);
  lynxtronModule.id = 'lynxtron';
  lynxtronModule.loaded = true;
  lynxtronModule.filename = name;
  Object.defineProperty(lynxtronModule, 'exports', {
    get: () => require('lynxtron'),
  });
  Module._cache[name] = lynxtronModule;
};

makeLynxtronModule('lynxtron');

const originalResolveFilename = Module._resolveFilename;

Module._resolveFilename = function (request, parent, isMain, options) {
  if (request === 'lynxtron') {
    return 'lynxtron';
  } else {
    return originalResolveFilename(request, parent, isMain, options);
  }
};

const bridgeData: APIS = {};

// --- Lynxtron BTS Env Setup ---
export function setupLynxtronBTS(console: Console, preload_paths: string[]) {
  // replace console
  globalThis.console = console;
  // @ts-ignore
  globalThis.__contextBridge.initModuleAPI(bridgeData);
  try {
    runPreloadScripts(preload_paths);
  } catch (e) {
    console.error('runPreloadScripts error: ', e);
  }
}

// --- Get Lynxtron BTS Bridge Data ---
export function getLynxtronBTSBridgeData() {
  return bridgeData;
}
