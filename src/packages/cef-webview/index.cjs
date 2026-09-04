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
  return nativeBinding.initialize(options);
}

const cefWebview = {
  initialize,
};

module.exports = cefWebview;
module.exports.default = cefWebview;
