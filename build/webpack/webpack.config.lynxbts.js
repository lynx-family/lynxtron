const path = require('node:path');

module.exports = (env, argv) => {
  const config = require('./webpack.config.base')({
    target: 'lynxbts'
  })(env, argv);

  config.entry = path.resolve(__dirname, '..', '..', 'lib', 'lynxbts', 'init.ts');
  config.output.library = {
    type: 'commonjs2'
  };

  return config;
};
