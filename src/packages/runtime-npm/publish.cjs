const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const { spawnSync } = require('node:child_process');
const pacote = require('pacote');

async function publish(directory, tag) {
  const { loadRegistryOptions } = await import('../lynxtron/runtime-package.js');
  const options = { ...await loadRegistryOptions(), registry: 'https://registry.npmjs.org/' };
  const entries = (await fs.readdir(directory)).sort();
  for (const entry of entries) {
    const packageDirectory = path.resolve(directory, entry);
    const local = JSON.parse(await fs.readFile(path.join(packageDirectory, 'package.json'), 'utf8'));
    const spec = `${local.name}@${local.version}`;
    let remote;
    try { remote = await pacote.manifest(spec, options); }
    catch (error) { if (error.code !== 'E404') throw error; }
    if (!remote) {
      const result = spawnSync('npm', ['publish', '--access', 'public', '--provenance', '--tag', tag,
        '--registry', options.registry], { cwd: packageDirectory, stdio: 'inherit' });
      if (result.error) throw result.error;
      if (result.status !== 0) throw new Error(`Failed to publish ${spec}`);
    }
    // Verify uploaded bytes as well as metadata before releasing the main package.
    const temporary = await fs.mkdtemp(path.join(os.tmpdir(), 'lynxtron-publish-'));
    try {
      await pacote.extract(spec, temporary, { ...options, preferOnline: true });
      const manifest = JSON.parse(await fs.readFile(path.join(temporary, 'package.json'), 'utf8'));
      const checksum = crypto.createHash('sha256').update(await fs.readFile(path.join(temporary, 'runtime.zip'))).digest('hex');
      if (checksum !== local.runtimeSha256 || manifest.name !== local.name ||
          manifest.version !== local.version || JSON.stringify(manifest.os) !== JSON.stringify(local.os) ||
          JSON.stringify(manifest.cpu) !== JSON.stringify(local.cpu)) {
        throw new Error(`Published package differs from this build: ${spec}. Use a new version.`);
      }
    } finally { await fs.rm(temporary, { recursive: true, force: true }); }
  }
}
publish(...process.argv.slice(2)).catch(error => { console.error(error.message); process.exitCode = 1; });
