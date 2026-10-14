const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const artifacts = require('../lynxtron/runtime-artifacts.cjs');

function prepare({ version, artifactsDirectory, outputDirectory }) {
  if (!/^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$/.test(version)) throw new Error('Invalid version');
  const allFiles = [];
  function walk(directory) {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
      const filename = path.join(directory, entry.name);
      if (entry.isDirectory()) walk(filename);
      else if (entry.isFile()) allFiles.push(filename);
    }
  }
  walk(artifactsDirectory);
  // Validate the complete matrix before creating publishable packages.
  const packages = artifacts.RUNTIME_PLATFORMS.flatMap(([platform, arch]) =>
    artifacts.VALID_RUNTIME_VARIANTS.map(variant => {
      const filename = artifacts.getRuntimeArtifactFilename({ platform, arch, version, variant });
      const matches = allFiles.filter(file => path.basename(file) === filename);
      if (matches.length !== 1) throw new Error(`Expected exactly one ${filename}, found ${matches.length}`);
      return { platform, arch, variant, archive: matches[0],
        name: artifacts.getRuntimePackageName({ platform, arch, variant }) };
    }));
  fs.mkdirSync(outputDirectory, { recursive: true });
  for (const pkg of packages) {
    const directory = path.join(outputDirectory, pkg.name.split('/')[1]);
    fs.mkdirSync(directory, { recursive: true });
    fs.copyFileSync(pkg.archive, path.join(directory, 'runtime.zip'));
    const runtimeSha256 = crypto.createHash('sha256').update(fs.readFileSync(pkg.archive)).digest('hex');
    fs.writeFileSync(path.join(directory, 'package.json'), JSON.stringify({
      name: pkg.name, version, description: `Lynxtron ${pkg.variant} runtime for ${pkg.platform}-${pkg.arch}`,
      os: [pkg.platform], cpu: [pkg.arch], files: ['runtime.zip', 'LICENSE'],
      license: 'Apache-2.0', repository: { type: 'git', url: 'https://github.com/lynx-family/lynxtron.git' },
      runtimeSha256, publishConfig: { access: 'public' },
    }, null, 2) + '\n');
    fs.copyFileSync(path.resolve(__dirname, '../../../LICENSE'), path.join(directory, 'LICENSE'));
    fs.writeFileSync(path.join(directory, 'README.md'), `# ${pkg.name}\n\nPlatform runtime archive for @lynx-js/lynxtron. Managed automatically by Lynxtron.\n`);
  }
  return packages;
}
module.exports = { prepare };
if (require.main === module) {
  const [version, artifactsDirectory, outputDirectory] = process.argv.slice(2);
  prepare({ version, artifactsDirectory, outputDirectory });
}
