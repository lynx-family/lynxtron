'use strict';

require('./module-resolver.js');

const electronBuilderCli = require.resolve('electron-builder/out/cli/cli.js', {
  paths: [__dirname],
});

const args = process.argv.slice(2);
if (args.length === 1 && (args[0] === '--version' || args[0] === '-v')) {
  // yargs otherwise discovers lynxtron-builder's package.json first.
  const manifest = require(
    require.resolve('electron-builder/package.json', { paths: [__dirname] })
  );
  process.stdout.write(`${manifest.version}\n`);
} else {
  process.argv[1] = electronBuilderCli;
  require(electronBuilderCli);
}
