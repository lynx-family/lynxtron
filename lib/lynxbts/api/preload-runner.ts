import { pathToFileURL } from 'url';

declare global {
  var __non_webpack_require__: (id: any) => unknown;
}

// --- Preload Scripts Runner ---
export default async function runPreloadScripts(preloadPaths: string[]) {
  const path = require('path');
  const Module = require('module') as NodeJS.ModuleInternal;
  const reportError = (script: string, err: unknown) => {
    console.error(`Unable to load preload script: ${script}`);
    console.error(err);
  };

  const cjsPreloads = preloadPaths.filter((p) => path.extname(p) !== '.mjs');
  const esmPreloads = preloadPaths.filter((p) => path.extname(p) === '.mjs');

  for (const script of cjsPreloads) {
    try {
      Module._load(script, Module, true);
    } catch (err) {
      reportError(script, err);
    }
  }

  if (esmPreloads.length) {
    const { runEntryPointWithESMLoader } = __non_webpack_require__(
      'internal/modules/run_main'
    ) as typeof import('@node/lib/internal/modules/run_main');
    await runEntryPointWithESMLoader(async (loader: any) => {
      for (const script of esmPreloads) {
        try {
          await loader.import(
            pathToFileURL(script).toString(),
            undefined,
            Object.create(null)
          );
        } catch (err) {
          reportError(script, err);
        }
      }
    });
  }
}
