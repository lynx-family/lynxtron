const path = require('node:path');

const packageRoot = __dirname;
const distRoot = path.join(packageRoot, 'dist');

function platformExecutableName() {
  if (process.platform === 'win32') {
    return 'lynxtron.exe';
  }

  if (process.platform === 'darwin') {
    return path.join('lynxtron.app', 'Contents', 'MacOS', 'lynxtron');
  }

  throw new Error(
    `lynxtron builds are not available on platform: ${process.platform}`
  );
}

const executablePath = path.join(distRoot, platformExecutableName());

const dllPath =
  process.platform === 'win32'
    ? path.join(distRoot, 'lynxtron.dll')
    : undefined;

const importLibraryPath =
  process.platform === 'win32'
    ? path.join(distRoot, 'lynxtron.dll.lib')
    : undefined;

module.exports = {
  packageRoot,
  distRoot,
  executablePath,
  dllPath,
  importLibraryPath,
};
