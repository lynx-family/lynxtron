import timers = require('timers');
import * as util from 'util';

import type * as stream from 'stream';

import { EventEmitter } from 'events';
// import * as fs from 'fs';
import * as path from 'path';

import type * as url from 'url';
import type * as v8 from 'v8';

// We modified the original process.argv to let node.js load the init.js,
// we need to restore it here.
process.argv.splice(1, 1);

type AnyFn = (...args: any[]) => any;

// setImmediate and process.nextTick makes use of uv_check and uv_prepare to
// run the callbacks, however since we only run uv loop on requests, the
// callbacks wouldn't be called until something else activated the uv loop,
// which would delay the callbacks for arbitrary long time. So we should
// initiatively activate the uv loop once setImmediate and process.nextTick is
// called.
const wrapWithActivateUvLoop = function <T extends AnyFn>(func: T): T {
  return wrap(func, function (func) {
    return function (this: any, ...args: any[]) {
      process.activateUvLoop();
      return func.apply(this, args);
    };
  }) as T;
};

/**
 * Casts to any below for func are due to Typescript not supporting symbols
 * in index signatures
 *
 * Refs: https://github.com/Microsoft/TypeScript/issues/1863
 */
function wrap<T extends AnyFn>(func: T, wrapper: (fn: AnyFn) => T) {
  const wrapped = wrapper(func);
  if ((func as any)[util.promisify.custom]) {
    (wrapped as any)[util.promisify.custom] = wrapper(
      (func as any)[util.promisify.custom]
    );
  }
  return wrapped;
}

// process.nextTick and setImmediate make use of uv_check and uv_prepare to
// run the callbacks, however since we only run uv loop on requests, the
// callbacks wouldn't be called until something else activated the uv loop,
// which would delay the callbacks for arbitrary long time. So we should
// initiatively activate the uv loop once process.nextTick and setImmediate is
// called.
process.nextTick = wrapWithActivateUvLoop(process.nextTick);
global.setImmediate = timers.setImmediate = wrapWithActivateUvLoop(
  timers.setImmediate
);
global.clearImmediate = timers.clearImmediate;

// setTimeout needs to update the polling timeout of the event loop, when
// called under Chromium's event loop the node's event loop won't get a chance
// to update the timeout, so we have to force the node's event loop to
// recalculate the timeout in the process.
timers.setTimeout = wrapWithActivateUvLoop(timers.setTimeout);
timers.setInterval = wrapWithActivateUvLoop(timers.setInterval);

// Update the global version of the timer apis to use the above wrapper
// only in the process that runs node event loop alongside chromium
// event loop. We skip renderer with nodeIntegration here because node globals
// are deleted in these processes, see renderer/init.js for reference.

global.setTimeout = timers.setTimeout;
global.setInterval = timers.setInterval;

if (process.platform === 'win32') {
  // Always returns EOF for stdin stream.
  const { Readable } = require('stream') as typeof stream;
  const stdin = new Readable();
  stdin.push(null);
  Object.defineProperty(process, 'stdin', {
    configurable: false,
    enumerable: true,
    get() {
      return stdin;
    },
  });
}

const Module = require('module') as NodeJS.ModuleInternal;

// Make a fake Electron module that we will insert into the module cache
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
makeLynxtronModule('lynxtron/main');

const originalResolveFilename = Module._resolveFilename;

// TODO(Guo Xi): we only got electron
// 'electron/{common,main,renderer,utility}' are module aliases
// of the 'electron' module for TypeScript purposes, i.e., the types for
// 'electron/main' consist of only main process modules, etc. It is intentional
// that these can be `require()`-ed from both the main process as well as the
// renderer process regardless of the names, they're superficial for TypeScript
// only.
const lynxtronModuleNames = new Set(['lynxtron', 'lynxtron/main']);
Module._resolveFilename = function (request, parent, isMain, options) {
  if (lynxtronModuleNames.has(request)) {
    return 'lynxtron';
  } else {
    return originalResolveFilename(request, parent, isMain, options);
  }
};

process
  ._linkedBinding('electron_browser_event_emitter')
  .setEventEmitterPrototype(EventEmitter.prototype);

// Emit 'exit' event on quit.
const { app } = require('lynxtron');

app.on('quit', (_event: Event, exitCode: number) => {
  process.emit('exit', exitCode);
});

// Map process.exit to app.exit, which quits gracefully.
process.exit = app.exit as () => never;

// Now we try to load app's package.json.
const v8Util = process._linkedBinding('electron_common_v8_util');
let packagePath = null;
let packageJson = null;
const searchPaths: string[] = v8Util.getHiddenValue(global, 'appSearchPaths');
const searchPathsOnlyLoadASAR: boolean = v8Util.getHiddenValue(
  global,
  'appSearchPathsOnlyLoadASAR'
);
// Borrow the _getOrCreateArchive asar helper
const getOrCreateArchive = process._getOrCreateArchive;
delete process._getOrCreateArchive;

if (process.resourcesPath) {
  for (packagePath of searchPaths) {
    try {
      packagePath = path.join(process.resourcesPath, packagePath);
      if (searchPathsOnlyLoadASAR) {
        if (!getOrCreateArchive?.(packagePath)) {
          continue;
        }
      }
      packageJson = Module._load(path.join(packagePath, 'package.json'));
      break;
    } catch {
      continue;
    }
  }
}

if (packageJson == null) {
  process.nextTick(function () {
    return process.exit(1);
  });
  throw new Error('Unable to find a valid app');
}

// Set application's version.
if (packageJson.version != null) {
  app.setVersion(packageJson.version);
}

// Set application's name.
if (packageJson.productName != null) {
  app.name = `${packageJson.productName}`.trim();
} else if (packageJson.name != null) {
  app.name = `${packageJson.name}`.trim();
}

// Set application's desktop name.
// if (packageJson.desktopName != null) {
//   app.setDesktopName(packageJson.desktopName);
// } else {
//   app.setDesktopName(`${app.name}.desktop`);
// }

// Set v8 flags, deliberately lazy load so that apps that do not use this
// feature do not pay the price
if (packageJson.v8Flags != null) {
  (require('v8') as typeof v8).setFlagsFromString(packageJson.v8Flags);
}

app.setAppPath(packagePath);

// Set main startup script of the app.
const mainStartupScript = packageJson.main || 'index.js';

// Quit when all windows are closed and no other one is listening to this.
app.on('window-all-closed', () => {
  if (app.listenerCount('window-all-closed') === 1) {
    app.quit();
  }
});

const { appCodeLoaded } = process;
delete process.appCodeLoaded;

if (packagePath) {
  // Finally load app's main.js and transfer control to C++.
  if (
    (packageJson.type === 'module' && !mainStartupScript.endsWith('.cjs')) ||
    mainStartupScript.endsWith('.mjs')
  ) {
    const { runEntryPointWithESMLoader } = __non_webpack_require__(
      'internal/modules/run_main'
    ) as typeof import('@node/lib/internal/modules/run_main');
    const main = (require('url') as typeof url).pathToFileURL(
      path.join(packagePath, mainStartupScript)
    );
    runEntryPointWithESMLoader(async (cascadedLoader: any) => {
      try {
        await cascadedLoader.import(
          main.toString(),
          undefined,
          Object.create(null)
        );
        appCodeLoaded!();
      } catch (err) {
        appCodeLoaded!();
        process.emit('uncaughtException', err as Error);
      }
    });
  } else {
    // Call appCodeLoaded before just for safety, it doesn't matter here as _load is synchronous
    appCodeLoaded!();
    Module._load(path.join(packagePath, mainStartupScript), Module, true);
  }
} else {
  console.error(
    'Failed to locate a valid package to load (app, app.asar or default_app.asar)'
  );
  console.error(
    "This normally means you've damaged the Electron package somehow"
  );
  appCodeLoaded!();
}
