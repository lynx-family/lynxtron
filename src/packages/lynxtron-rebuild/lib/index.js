import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
import https from 'node:https';
import { pipeline } from 'node:stream/promises';
import { spawn } from 'node:child_process';
import { zip } from 'compressing';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const BASE_URL = '';

function getLynxtronVersion() {
  let pkgPath;
  
  try {
    pkgPath = path.join(process.cwd(), 'node_modules', '@lynx-js', 'lynxtron', 'package.json');
    const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
    return pkg.version;
  } catch (e) {
    try {
      pkgPath = path.join(__dirname, '..', '..', '..', 'node_modules', '@lynx-js', 'lynxtron', 'package.json');
      const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
      return pkg.version;
    } catch (e2) {
      throw new Error('Could not find @lynx-js/lynxtron package');
    }
  }
}

async function downloadFile(url, destPath) {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(destPath);
    https.get(url, (response) => {
      if (response.statusCode !== 200) {
        fs.unlink(destPath, () => {});
        reject(new Error(`Failed to download ${url}, status: ${response.statusCode}`));
        return;
      }
      pipeline(response, file)
        .then(resolve)
        .catch(reject);
    }).on('error', (err) => {
      fs.unlink(destPath, () => {});
      reject(err);
    });
  });
}

async function extractZip(zipPath, destDir) {
  await zip.uncompress(zipPath, destDir);
}

async function downloadHeaders(version, distDir) {
  const url = `${BASE_URL}v${version}/lynxtron-v${version}-node-headers.zip`;
  const zipPath = path.join(distDir, `lynxtron-v${version}-node-headers.zip`);
  const headersPath = path.join(distDir, `v${version}`);
  
  if (fs.existsSync(headersPath)) {
    console.log(`Headers already exist at ${headersPath}, skipping download`);
    return headersPath;
  }
  
  if (!fs.existsSync(distDir)) {
    fs.mkdirSync(distDir, { recursive: true });
  }
  
  console.log(`Downloading headers from ${url}`);
  await downloadFile(url, zipPath);
  
  console.log(`Extracting headers to ${distDir}`);
  await extractZip(zipPath, distDir);
  
  const extractedDir = path.join(distDir, 'node_headers');
  if (fs.existsSync(extractedDir)) {
    fs.renameSync(extractedDir, headersPath);
  }
  
  fs.unlinkSync(zipPath);
  
  return headersPath;
}

function findModulesWithNativeCode(buildPath) {
  const modules = [];
  const nodeModulesPath = path.join(buildPath, 'node_modules');
  
  if (!fs.existsSync(nodeModulesPath)) {
    return modules;
  }
  
  const packages = fs.readdirSync(nodeModulesPath, { withFileTypes: true });
  
  for (const pkg of packages) {
    if (!pkg.isDirectory()) continue;
    if (pkg.name.startsWith('.')) continue;
    
    const pkgPath = path.join(nodeModulesPath, pkg.name);
    const bindingGypPath = path.join(pkgPath, 'binding.gyp');
    
    if (fs.existsSync(bindingGypPath)) {
      modules.push(pkgPath);
    }
  }
  
  return modules;
}

async function rebuildModule(modulePath, headersDir, electronVersion, arch) {
  return new Promise((resolve, reject) => {
    const args = [
      'rebuild',
      `--target=${electronVersion}`,
      '--runtime=electron',
      `--arch=${arch}`,
      '--build-from-source',
      '--nodedir', headersDir
    ];
    
    console.log(`Rebuilding ${path.basename(modulePath)}...`);
    
    const nodeGyp = spawn('npx', ['node-gyp', ...args], {
      cwd: modulePath,
      stdio: 'inherit'
    });
    
    nodeGyp.on('error', (err) => {
      reject(err);
    });
    
    nodeGyp.on('close', (code) => {
      if (code === 0) {
        console.log(`Successfully rebuilt ${path.basename(modulePath)}`);
        resolve();
      } else {
        reject(new Error(`node-gyp failed to rebuild ${path.basename(modulePath)} with exit code ${code}`));
      }
    });
  });
}

export async function lynxtronRebuild(options = {}) {
  const version = getLynxtronVersion();
  console.log(`Detected Lynxtron version: ${version}`);
  
  const arch = options.arch || process.arch;
  console.log(`Using architecture: ${arch}`);
  
  const distDir = path.join(__dirname, '..', 'dist');
  const headersDir = await downloadHeaders(version, distDir);
  
  console.log(`Headers downloaded to: ${headersDir}`);
  
  const buildPath = options.buildPath || process.cwd();
  const modules = findModulesWithNativeCode(buildPath);
  
  if (modules.length === 0) {
    console.log('No native modules found to rebuild');
    return;
  }
  
  console.log(`Found ${modules.length} native module(s) to rebuild:`);
  modules.forEach(m => console.log(`  - ${path.basename(m)}`));
  
  for (const modulePath of modules) {
    try {
      await rebuildModule(modulePath, headersDir, version, arch);
    } catch (err) {
      console.error(`Error rebuilding ${path.basename(modulePath)}: ${err.message}`);
      throw err;
    }
  }
  
  console.log('Rebuild completed successfully!');
}