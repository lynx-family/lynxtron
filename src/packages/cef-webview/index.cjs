const path = require('path');
const manifest = require('./lynx.lib.json');

const target = manifest.platforms.lynxtron.targets.find(
  ({ os, arch }) => os === process.platform && arch === process.arch
);

if (!target) {
  throw new Error(
    `@lynx-js/cef-webview does not provide a binary for ${process.platform}/${process.arch}`
  );
}

const binaryPaths = Array.isArray(target.files)
  ? target.files.filter((file) => path.extname(file) === '.node')
  : [];
if (binaryPaths.length === 0) {
  throw new Error(
    `@lynx-js/cef-webview does not provide a binary for ${process.platform}/${process.arch}`
  );
}
if (process.platform === 'win32') {
  const runtimeDirectory = path.dirname(
    path.resolve(__dirname, binaryPaths[0])
  );
  process.env.PATH = `${runtimeDirectory};${process.env.PATH || ''}`;
}
const nativeBindings = binaryPaths.map((binaryPath) =>
  require(path.resolve(__dirname, binaryPath))
);
const nativeBinding = nativeBindings.find(
  (binding) => typeof binding.initialize === 'function'
);

if (!nativeBinding) {
  throw new Error(
    `@lynx-js/cef-webview did not load an initialize-capable binary for ${process.platform}/${process.arch}`
  );
}

let initializedSettings;

function initialize(options = {}) {
  if (!options || typeof options !== 'object' || Array.isArray(options)) {
    throw new TypeError('initialize options must be an object');
  }
  for (const key of Object.keys(options)) {
    if (key !== 'storagePath' && key !== 'persistent') {
      throw new TypeError(`Unknown initialize option: ${key}`);
    }
  }
  if (options.persistent !== undefined && typeof options.persistent !== 'boolean') {
    throw new TypeError('persistent must be a boolean');
  }
  const { app } = require('lynxtron');
  if (!app.isReady()) {
    throw new Error('CEF must be initialized after app.whenReady()');
  }
  const storagePath = options.storagePath === undefined
    ? path.join(app.getPath('userData'), 'cef-webview')
    : options.storagePath;
  if (typeof storagePath !== 'string' || storagePath.includes('\0') ||
      !path.isAbsolute(storagePath)) {
    throw new TypeError('storagePath must be an absolute path without NUL characters');
  }
  const rootCachePath = path.normalize(storagePath);
  const settings = {
    rootCachePath,
    cachePath: options.persistent ? path.join(rootCachePath, 'profile') : '',
  };
  if (initializedSettings) {
    if (initializedSettings.rootCachePath !== settings.rootCachePath ||
        initializedSettings.cachePath !== settings.cachePath) {
      throw new Error('CEF is already initialized with different storage settings');
    }
    return true;
  }
  if (nativeBinding.initialize(settings) !== true) {
    throw new Error('CEF initialization failed');
  }
  initializedSettings = settings;
  return true;
}

const cefWebview = {
  initialize,
};

module.exports = cefWebview;
module.exports.default = cefWebview;
