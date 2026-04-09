
const path = require('path')

const { registerGlobalEnvModule } = process._linkedBinding("lynx_extension");

const extension_module = require(path.join(__dirname,
  'build',
  'Release',
  'lynx_demo_extension.node'
));

const setUp = () => {
  const creator = extension_module.getModuleCreator();
  if (creator && registerGlobalEnvModule) {
    registerGlobalEnvModule(creator.name, creator.creatorModuleFunc, creator.isLazyCreate, creator.opaque);
  } else {
    throw "lynx extension config is empty"
  }
}

exports.setUp = setUp;
