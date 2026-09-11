import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';

for (const platform of ['win32', 'darwin']) {
  test(`stages the ${platform} npm runtime payload`, {
    skip: platform === 'darwin' && process.platform === 'win32',
  }, async (t) => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'cef-stage-'));
    t.after(() => fs.rm(root, { recursive: true, force: true }));
    const pkg = path.join(root, 'src/packages/cef-webview');
    const write = async (relative, content = 'fixture') => {
      const file = path.join(root, relative);
      await fs.mkdir(path.dirname(file), { recursive: true });
      await fs.writeFile(file, content);
    };
    await write('src/packages/cef-webview/package.json', '{"type":"module"}');
    for (const script of ['stage-build.js', 'framework-links.js']) {
      await write(`src/packages/cef-webview/scripts/${script}`,
        await fs.readFile(new URL(`../scripts/${script}`, import.meta.url)));
    }
    await write('src/packages/cef-webview/build/Release/cef_extension.node');
    if (platform === 'win32') {
      await write('src/packages/cef-webview/build/Release/cef_subprocess.exe');
      await write('third_party/cef_binary/Release/libcef.dll');
      await write('third_party/cef_binary/Release/libcef.lib');
      await write('third_party/cef_binary/Resources/locales/en-US.pak');
    } else {
      const frameworks = 'src/packages/cef-webview/build/frameworks/Contents/Frameworks';
      const version = `${frameworks}/Chromium Embedded Framework.framework/Versions/A`;
      await write(`${version}/Chromium Embedded Framework`);
      await write(`${version}/Resources/icudtl.dat`);
      await write(`${version}/Libraries/libEGL.dylib`);
      await write(`${frameworks}/LynxtronWebview Helper.app/Contents/MacOS/LynxtronWebview Helper`);
    }
    const result = spawnSync(process.execPath, [path.join(pkg, 'scripts/stage-build.js')], {
      env: { ...process.env, npm_config_platform: platform, npm_config_arch: 'x64' },
      encoding: 'utf8',
    });
    assert.equal(result.status, 0, result.stderr);
    const output = path.join(pkg, 'dist', platform, 'x64');
    assert.equal(await fs.readFile(path.join(output, 'cef_extension.node'), 'utf8'), 'fixture');
    if (platform === 'win32') {
      for (const file of ['cef_subprocess.exe', 'libcef.dll', 'locales/en-US.pak']) {
        assert.equal(await fs.readFile(path.join(output, file), 'utf8'), 'fixture');
      }
      await assert.rejects(fs.stat(path.join(output, 'libcef.lib')), { code: 'ENOENT' });
    } else {
      assert.equal(await fs.readFile(path.join(output,
        'frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework'), 'utf8'), 'fixture');
      assert.equal(await fs.readlink(path.join(output,
        'frameworks/Chromium Embedded Framework.framework/Versions/Current')), 'A');
      assert.equal(await fs.readFile(path.join(output,
        'frameworks/LynxtronWebview Helper.app/Contents/MacOS/LynxtronWebview Helper'), 'utf8'), 'fixture');
    }
  });
}
