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

function initialize(options = {}) {
  // The current implementation shares the default CEF storage directory across
  // host applications. Only one host process can initialize the CEF browser at
  // a time; that process can create multiple WebViews. CEF helper subprocesses
  // are not additional browser host processes.
  const result = nativeBinding.initialize(options);
  if (result === false) {
    throw new Error(
      'CEF initialization failed. The current version does not support ' +
        'multiple host processes using WebView at the same time. ' +
        'Close other applications running WebView and try again. ' +
        'Initialization can also fail for other reasons; check the native logs.'
    );
  }
  return result;
}

const cefWebview = {
  initialize,
};

module.exports = cefWebview;
module.exports.default = cefWebview;
