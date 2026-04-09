
const path = require('path')
const { registerGlobalEnvModule } = process._linkedBinding("lynx_extension");
const platform = process.platform;
const arch = process.arch;
const extension_module = require(path.join(__dirname,
  'dist',
  platform,
  arch,
  'cef_extension.node'
));

const setUp = () => {
  const creator = extension_module.getExtensionConfig();
  if (creator && registerGlobalEnvModule) {
    registerGlobalEnvModule(creator.name, creator.creatorModuleFunc, creator.isLazyCreate, creator.opaque);
  } else {
    throw "lynx extension config is empty"
  }
  extension_module.initialize();
}
exports.setUp = setUp;