
const path = require('path')
const { registerGlobalEnvModule } = process._linkedBinding("lynx_extension");
const extension_module = require(path.join(__dirname,
  'darwin',
  'arm64',
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