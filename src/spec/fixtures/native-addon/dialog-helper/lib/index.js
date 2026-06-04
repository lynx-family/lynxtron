try {
  module.exports = require('../build/Release/dialog_helper.node');
} catch {
  module.exports = require('../build/Debug/dialog_helper.node');
}
