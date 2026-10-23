const test = require('node:test');
const assert = require('node:assert/strict');
const { execFileSync } = require('node:child_process');
const Module = require('node:module');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const builderDir = path.resolve(__dirname, '..');
const resolverPath = path.join(builderDir, 'module-resolver.js');
const vendorDir = fs.realpathSync(path.join(builderDir, 'vendor'));
const electronBuilderDir = path.dirname(
  require.resolve('electron-builder/package.json', { paths: [builderDir] })
);

require(resolverPath);

function fakeParent(filename) {
  const parent = new Module(filename);
  parent.filename = filename;
  parent.paths = Module._nodeModulePaths(path.dirname(filename));
  return parent;
}

function resolveFrom(filename, request) {
  return Module._resolveFilename(request, fakeParent(filename), false, {});
}

test('electron-builder callers resolve the patched vendor copy', () => {
  const resolved = resolveFrom(
    path.join(electronBuilderDir, 'out', 'index.js'),
    'app-builder-lib'
  );
  assert.ok(resolved.startsWith(`${vendorDir}${path.sep}`), resolved);
});

test('electron-builder-squirrel-windows shares the patched copy', () => {
  const resolved = resolveFrom(
    path.join(
      os.tmpdir(),
      'node_modules',
      'electron-builder-squirrel-windows',
      'out',
      'index.js'
    ),
    'app-builder-lib'
  );
  assert.ok(resolved.startsWith(`${vendorDir}${path.sep}`), resolved);
});

test('user hooks keep their own module resolution (not hijacked)', () => {
  // A caller outside electron-builder must get the normally resolved copy, never vendor/.
  const resolved = resolveFrom(
    path.join(builderDir, 'user-hook.js'),
    'app-builder-lib'
  );
  assert.ok(!resolved.startsWith(`${vendorDir}${path.sep}`), resolved);
});

test('the resolver is inherited by forked helper processes', (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'lynxtron-builder-fork-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  const script = path.join(dir, 'fork.js');
  // electron-builder forks helpers (e.g. remote-rebuild.js); Node propagates
  // execArgv, so the child must load the resolver too.
  fs.writeFileSync(
    script,
    `const { fork } = require('node:child_process');
if (process.send) {
  process.send(require('node:module')._resolveFilename.name);
} else {
  const child = fork(__filename);
  child.on('message', (name) => { console.log(name); child.disconnect(); });
}
`
  );

  const output = execFileSync(
    process.execPath,
    ['--require', resolverPath, script],
    { encoding: 'utf8' }
  ).trim();
  assert.equal(output, 'resolveLynxtronPackage');
});
