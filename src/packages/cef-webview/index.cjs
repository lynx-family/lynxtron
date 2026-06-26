
const path = require('path');
const platform = process.platform;
const arch = process.arch;

const nativeBinding = require(path.join(
  __dirname,
  'dist',
  platform,
  arch,
  'cef_extension.node',
));

function initialize(options = {}) {
  return nativeBinding.initialize(options);
}

const cefWebview = {
  initialize,
};

module.exports = cefWebview;
module.exports.default = cefWebview;
