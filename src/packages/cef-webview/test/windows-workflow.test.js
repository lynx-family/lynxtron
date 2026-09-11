import assert from 'node:assert/strict';
import fs from 'node:fs';
import test from 'node:test';

const readRoot = (file) =>
  fs.readFileSync(new URL(`../../../../${file}`, import.meta.url), 'utf8');

test('Windows CI and release call the package build wrapper after the runtime build', () => {
  for (const file of ['.github/workflows/ci.yml', '.github/workflows/publish.yml']) {
    const workflow = readRoot(file);
    const build = workflow.indexOf('build_cef_webview.ps1');
    assert.ok(build > workflow.indexOf('uses: ./lynxtron/.github/actions/windows-lynxtron-build'));
    assert.ok(build !== -1);
  }
  const script = readRoot('lynxtron_tools/build_cef_webview.ps1');
  assert.ok(script.includes("out\\Release\\lynxtron.dll.lib"));
  assert.ok(script.includes('LYNXTRON_IMPORT_LIB = $ImportLibrary'));
  assert.ok(script.includes('workspace @lynx-js/cef-webview build'));
  assert.ok(script.indexOf('hab.ps1') < script.indexOf('workspace @lynx-js/cef-webview build'));
  assert.match(script, /powershell\.exe[^\r\n]*-File "\$PSScriptRoot\\hab\.ps1" sync/);
  assert.ok(script.includes("if ($LASTEXITCODE -ne 0) { throw 'Failed to sync the CEF extension dependencies' }"));
  for (const artifact of ['cef_extension.node', 'cef_subprocess.exe', 'libcef.dll']) {
    assert.ok(script.includes(artifact));
  }
  const cmake = fs.readFileSync(new URL('../CMakeLists.txt', import.meta.url), 'utf8');
  assert.ok(cmake.includes('lynx_link_lynxtron_runtime'));
  assert.ok(!cmake.includes('lxtn.dll.lib'));
});
