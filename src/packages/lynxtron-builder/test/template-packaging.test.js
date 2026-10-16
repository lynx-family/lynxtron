const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const yaml = require('js-yaml');

for (const name of ['lynxtron-shell-demo']) {
  test(`${name} packages the application directory only once`, () => {
    const packageRoot = path.resolve(__dirname, '../..', name);
    const config = yaml.load(
      fs.readFileSync(path.join(packageRoot, 'electron-builder.yml'), 'utf8')
    );
    assert.equal(config.directories.app, 'dist/desktop');
    // App files are packaged through directories.app/files. Copying the same
    // tree through extraResources duplicates all staged native payloads.
    for (const copy of config.extraResources || []) {
      assert.notEqual(
        path.resolve(packageRoot, copy.from),
        path.resolve(packageRoot, config.directories.app)
      );
    }
    assert.deepEqual(config.extraResources, [
      { from: 'src/assets', to: 'assets' },
    ]);
  });
}

test('create-lynxtron generates its template from the deduplicated shell demo', () => {
  const generator = fs.readFileSync(
    path.resolve(
      __dirname,
      '../../create-lynxtron/scripts/generate-template.js'
    ),
    'utf8'
  );
  assert.match(
    generator,
    /const srcDir = path.resolve\(pkgDir, '..', 'lynxtron-shell-demo'\)/
  );
});
