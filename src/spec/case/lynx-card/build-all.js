#!/usr/bin/env node

const childProcess = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const projectRoot = __dirname;
const distDir = path.resolve(projectRoot, 'dist');

const { entries } = require('./cards.config');
const allProjects = Object.keys(entries);

let selectedProject = null;
for (let i = 2; i < process.argv.length; i++) {
  const arg = process.argv[i];
  if (arg === '--project') {
    selectedProject = process.argv[i + 1] || null;
    break;
  }
  if (arg.startsWith('--project=')) {
    selectedProject = arg.slice('--project='.length) || null;
    break;
  }
}

if (!selectedProject) {
  const maybePositional = process.argv[2];
  if (maybePositional && !maybePositional.startsWith('-')) {
    selectedProject = maybePositional;
  }
}

selectedProject =
  selectedProject ||
  process.env.npm_config_project ||
  process.env.LYNX_CARD ||
  null;

if (selectedProject && !allProjects.includes(selectedProject)) {
  process.stderr.write(
    `Unknown project "${selectedProject}", expected one of: [${allProjects.join(', ')}]\n if new project, please add it to cards.config.js\n`
  );
  process.exit(1);
}

const projects = selectedProject ? [selectedProject] : allProjects;

// Resolve rspeedy from this package's own node_modules so the build always uses
// the version pinned here, regardless of what a parent PATH (e.g. the repo-root
// node_modules/.bin injected by `npm run test`) might shadow it with. The package
// only exports "./package.json", so locate that and derive the bin from there.
const rspeedyPkgPath = require.resolve('@lynx-js/rspeedy/package.json', {
  paths: [projectRoot],
});
const rspeedyBin = path.resolve(
  path.dirname(rspeedyPkgPath),
  require(rspeedyPkgPath).bin.rspeedy
);

if (!selectedProject) {
  fs.rmSync(distDir, { recursive: true, force: true });
} else if (!fs.existsSync(distDir)) {
  fs.mkdirSync(distDir, { recursive: true });
}

for (const project of projects) {
  const result = childProcess.spawnSync(process.execPath, [rspeedyBin, 'build'], {
    cwd: projectRoot,
    stdio: 'inherit',
    env: {
      ...process.env,
      LYNX_CARD: project,
    },
  });

  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}
