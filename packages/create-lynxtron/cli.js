#!/usr/bin/env node
import fs from 'fs';
import fsp from 'fs/promises';
import path from 'path';
import { fileURLToPath } from 'url';
import process from 'process';
import prompts from 'prompts';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

async function ensureDir(p) {
  await fsp.mkdir(p, { recursive: true });
}

async function copyDir(src, dest) {
  const entries = await fsp.readdir(src, { withFileTypes: true });
  for (const entry of entries) {
    if (entry.name === 'node_modules' || entry.name === 'dist' || entry.name === '.git') continue;
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      await ensureDir(destPath);
      await copyDir(srcPath, destPath);
    } else {
      await fsp.copyFile(srcPath, destPath);
    }
  }
}

async function updatePackageName(dest, name) {
  const pkgPath = path.join(dest, 'package.json');
  try {
    const buf = await fsp.readFile(pkgPath, 'utf8');
    const pkg = JSON.parse(buf);
    pkg.name = name;
    delete pkg.author;
    delete pkg.description;
    await fsp.writeFile(pkgPath, JSON.stringify(pkg, null, 2) + '\n', 'utf8');
  } catch {}
}

async function main() {
  let targetArg = process.argv[2];
  if (!targetArg) {
    const answers = await prompts(
      [
        {
          type: 'text',
          name: 'project',
          message: 'Project name or path',
          initial: 'lynxtron-app'
        }
      ],
      {
        onCancel: () => {
          process.exit(0);
        }
      }
    );
    targetArg = answers.project || 'lynxtron-app';
  }
  const targetDir = path.resolve(process.cwd(), targetArg);
  const appName = path.basename(targetDir);
  let templateDir = path.resolve(__dirname, 'dist', 'lynxtron-shell-demo');
  if (!fs.existsSync(templateDir)) {
    const siblingDist = path.resolve(__dirname, '..', 'dist', 'lynxtron-shell-demo');
    if (fs.existsSync(siblingDist)) {
      templateDir = siblingDist;
    } else {
      const rootTemplate = path.resolve(__dirname, 'lynxtron-shell-demo');
      const siblingRootTemplate = path.resolve(__dirname, '..', 'lynxtron-shell-demo');
      if (fs.existsSync(rootTemplate)) {
        templateDir = rootTemplate;
      } else if (fs.existsSync(siblingRootTemplate)) {
        templateDir = siblingRootTemplate;
      } else {
        console.error('Template not found:', templateDir);
        process.exit(1);
      }
    }
  }
  if (fs.existsSync(targetDir)) {
    const hasFiles = (await fsp.readdir(targetDir)).length > 0;
    if (hasFiles) {
      const ans = await prompts(
        [
          {
            type: 'confirm',
            name: 'overwrite',
            message: `Directory already exists: ${targetDir}. Overwrite?`,
            initial: false
          }
        ],
        {
          onCancel: () => {
            process.exit(0);
          }
        }
      );
      if (!ans.overwrite) {
        console.log('Cancelled');
        process.exit(0);
      }
      await fsp.rm(targetDir, { recursive: true, force: true });
    }
  }
  await ensureDir(targetDir);
  await copyDir(templateDir, targetDir);
  await updatePackageName(targetDir, appName);
  try {
    const pkgPath = path.join(targetDir, 'package.json');
    const pkg = JSON.parse(await fsp.readFile(pkgPath, 'utf8'));
    let versions = {};
    try {
      versions = JSON.parse(await fsp.readFile(path.resolve(__dirname, 'dist', 'versions.json'), 'utf8'));
    } catch {
      try {
        versions = JSON.parse(await fsp.readFile(path.resolve(__dirname, 'versions.json'), 'utf8'));
      } catch {}
    }
    const replaceWorkspace = (deps) => {
      if (!deps) return;
      for (const [name, ver] of Object.entries(deps)) {
        if (typeof ver === 'string' && ver.startsWith('workspace:')) {
          const mapped = versions[name];
          if (mapped) deps[name] = mapped;
        }
      }
    };
    replaceWorkspace(pkg.dependencies);
    replaceWorkspace(pkg.devDependencies);
    replaceWorkspace(pkg.optionalDependencies);
    await fsp.writeFile(pkgPath, JSON.stringify(pkg, null, 2) + '\n', 'utf8');
  } catch {}
  console.log('Created Lynxtron app at', targetDir);
  console.log('Next steps:');
  const relDir = path.relative(process.cwd(), targetDir) || '.';
  console.log('  cd ' + '"' + relDir + '"');
  console.log('  npm install');
  console.log('  npm run dev');
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
