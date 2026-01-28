#!/usr/bin/env node
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');
const { makeUniversalApp } = require('@electron/universal');

const projectRoot = process.cwd();
const configPath = path.join(projectRoot, 'electron-builder.yml');
const tempConfigPath = path.join(projectRoot, 'config.json');

function getLynxtronVersion() {
  try {
    const lynxtronPackageJsonPath = require.resolve('lynxtron/package.json', { paths: [projectRoot] });
    const lynxtronPackageJson = JSON.parse(fs.readFileSync(lynxtronPackageJsonPath, 'utf8'));

    if (lynxtronPackageJson.version) {
      return lynxtronPackageJson.version;
    }
  } catch (e) {
    console.warn('Could not resolve installed lynxtron version, falling back to package.json.');
  }

  throw new Error("Failed to determine lynxtron version. Please check your package.json or node_modules.");
}


function isUniversalFromConfig(config) {
  if (!config || !config.mac || !config.mac.target) {
    return false;
  }
  const targets = config.mac.target;
  if (typeof targets === 'string') {
    return targets === 'universal';
  }
  if (Array.isArray(targets)) {
    return targets.some(t => {
      if (typeof t === 'string') {
        return t === 'universal';
      }
      if (typeof t === 'object' && t !== null) {
        return t.arch === 'universal';
      }
      return false;
    });
  }
  return false;
}

async function build() {
  const args = process.argv.slice(2);

  let config = {};
  if (fs.existsSync(configPath)) {
    const yamlContent = fs.readFileSync(configPath, 'utf8');
    config = yaml.load(yamlContent) || {};
  }

  const isUniversal = args.includes('--universal') || isUniversalFromConfig(config);

  try {
    if (isUniversal) {
      await runBuild('--x64');
      await runBuild('--arm64');
      await makeUniversal();
    } else {
      await runBuild();
    }
  } finally {
    if (fs.existsSync(tempConfigPath)) {
      fs.unlinkSync(tempConfigPath);
    }
  }
}

function runBuild(arch) {
  return new Promise((resolve, reject) => {
    let config = {};
    if (fs.existsSync(configPath)) {
      const yamlContent = fs.readFileSync(configPath, 'utf8');
      config = yaml.load(yamlContent);
    }

    const isUniversalBuild = process.argv.includes('--universal') || isUniversalFromConfig(config);

    // If we are building a slice of a universal build, override the arch settings in the config
    if (isUniversalBuild && arch) {
      const currentArch = arch.replace('--', '');
      if (config.mac) {
        // Override top-level mac.arch
        if (config.mac.arch === 'universal') {
          config.mac.arch = currentArch;
        }

        // Handle mac.target array
        if (Array.isArray(config.mac.target)) {
          config.mac.target = config.mac.target.map(t => {
            if (typeof t === 'object' && t !== null && t.arch === 'universal') {
              return { ...t, arch: currentArch };
            }
            if (t === 'universal') return null; // remove 'universal' string from targets
            return t;
          }).filter(t => t !== null);
          // If the array becomes empty, remove it to avoid issues.
          if (config.mac.target.length === 0) {
            delete config.mac.target;
          }
        }
        // Handle mac.target string
        else if (typeof config.mac.target === 'string' && config.mac.target === 'universal') {
          // If it's just 'universal', remove it and let electron-builder use defaults for the target type (e.g. dmg).
          delete config.mac.target;
        }
      }
    }

    if (!config.electronDownload) {
      const lynxtronVersion = getLynxtronVersion();
      const archFlag = arch || (process.argv.includes('--arm64') ? 'arm64' : 'x64');
      const resolvedArch = archFlag.replace('--', '');
      config.electronVersion = lynxtronVersion;
      config.electronDownload = {
        version: lynxtronVersion,
        // TODO(zhengsenyao): change to github release url
        mirror: 'https://tosv.byted.org/obj/lynx/lynxtron/',
        customDir: `v${lynxtronVersion}`,
        customFilename: `lynxtron-v${lynxtronVersion}-darwin-${resolvedArch}.zip`,
      };
    }
    fs.writeFileSync(tempConfigPath, JSON.stringify(config, null, 2));

    const electronBuilderPath = require.resolve('electron-builder/out/cli/cli.js');
    const args = process.argv.slice(2).filter(arg => arg !== '--universal');
    const finalArgs = ['-c', tempConfigPath, ...args];
    if (arch) {
      finalArgs.push(arch);
    }
    
    const child = spawn('node', [electronBuilderPath, ...finalArgs], {
      stdio: 'inherit',
    });

    child.on('close', (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`Build failed with code ${code}`));
      }
    });
  });
}

async function makeUniversal() {
  let config = {};
  if (fs.existsSync(configPath)) {
    const yamlContent = fs.readFileSync(configPath, 'utf8');
    config = yaml.load(yamlContent);
  }

  const x64AppPath = path.resolve(projectRoot, config.directories.output, 'mac', `${config.productName}.app`);
  const arm64AppPath = path.resolve(projectRoot, config.directories.output, 'mac-arm64', `${config.productName}.app`);
  const outAppPath = path.resolve(projectRoot, config.directories.output, 'mac-universal', `${config.productName}.app`);

  await makeUniversalApp({
    x64AppPath,
    arm64AppPath,
    outAppPath,
  });

  return new Promise((resolve, reject) => {
    const electronBuilderPath = require.resolve('electron-builder/out/cli/cli.js');
    const args = ['--mac', '--prepackaged', outAppPath, '--universal'];

    const child = spawn('node', [electronBuilderPath, ...args], {
      stdio: 'inherit',
    });

    child.on('close', (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`Universal packaging failed with code ${code}`));
      }
    });
  });
}

build().catch(err => {
  console.error(err);
  process.exit(1);
});
