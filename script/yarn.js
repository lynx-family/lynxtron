const cp = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const YARN_VERSION = /yarn_version = '(.+?)'/.exec(fs.readFileSync(path.resolve(__dirname, '../dependencies/DEPS'), 'utf8'))[1];
const NPX_CMD = process.platform === 'win32' ? 'npx.cmd' : 'npx';

const getYarnCommand = (version) => {
  if (version.startsWith('1')) {
    return [NPX_CMD, [`yarn@${version}`]];
  }
  return ['corepack', [`yarn@${version}`]];
};

if (require.main === module) {
  const [cmd, args] = getYarnCommand(YARN_VERSION);
  const child = cp.spawn(cmd, [...args, ...process.argv.slice(2)], {
    stdio: 'inherit',
    env: {
      ...process.env,
      npm_config_yes: 'true'
    },
    shell: process.platform === 'win32'
  });

  child.on('exit', code => process.exit(code));
}

exports.YARN_VERSION = YARN_VERSION;
exports.getYarnCommand = getYarnCommand;
