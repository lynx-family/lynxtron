
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

const cefXWebview = {
  initialize,
};

module.exports = cefXWebview;
module.exports.default = cefXWebview;
