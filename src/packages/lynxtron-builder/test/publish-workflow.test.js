const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const yaml = require('js-yaml');

const workflowPath = path.resolve(__dirname, '../../../../.github/workflows/publish.yml');
const workflowSource = fs.readFileSync(workflowPath, 'utf8');
const workflow = yaml.load(workflowSource);
const { jobs } = workflow;
const releaseWorkflowPath = path.resolve(
  __dirname,
  '../../../../.github/workflows/release.yml'
);
const releaseWorkflow = yaml.load(fs.readFileSync(releaseWorkflowPath, 'utf8'));
const changesetConfig = JSON.parse(
  fs.readFileSync(path.resolve(__dirname, '../../../.changeset/config.json'), 'utf8')
);
const workspacePackage = JSON.parse(
  fs.readFileSync(path.resolve(__dirname, '../../../package.json'), 'utf8')
);
const publishPrepareSource = fs.readFileSync(
  path.resolve(__dirname, '../../publish_prepare.py'),
  'utf8'
);
const ciWorkflowPath = path.resolve(__dirname, '../../../../.github/workflows/ci.yml');
const ciJobs = yaml.load(fs.readFileSync(ciWorkflowPath, 'utf8')).jobs;
const windowsBuildActionPath = path.resolve(
  __dirname,
  '../../../../.github/actions/windows-lynxtron-build/action.yml'
);
const windowsBuildAction = yaml.load(
  fs.readFileSync(windowsBuildActionPath, 'utf8')
);
const windowsEnvSetupSource = fs.readFileSync(
  path.resolve(__dirname, '../../../../lynxtron_tools/envsetup.ps1'),
  'utf8'
);

test('one version and one release gate both runtime variants', () => {
  assert.deepEqual(Object.keys(jobs['get-version'].outputs).sort(), [
    'prerelease',
    'source_sha',
    'tag',
    'version',
  ]);
  assert.equal(jobs['create-release-dev'], undefined);
  assert.equal(jobs['publish-npm-dev'], undefined);
  assert.deepEqual(
    new Set(jobs['create-release'].needs),
    new Set([
      'get-version',
      'build-cef-webview-macos',
      'build-macos',
      'build-linux',
      'build-windows',
      'build-macos-devtool',
      'build-linux-devtool',
      'build-windows-devtool',
    ])
  );
});

test('every publish job uses the immutable revision resolved from the requested source ref', () => {
  const sourceInput = workflow.on.workflow_dispatch.inputs.source_ref;
  assert.equal(sourceInput.required, true);
  assert.equal(sourceInput.default, 'main');

  const resolveStep = jobs['get-version'].steps.find(
    (step) => step.name === 'Resolve source revision'
  );
  assert.equal(resolveStep.with.ref, '${{ inputs.source_ref }}');

  const checkoutJobs = [
    'build-macos',
    'build-linux',
    'build-windows',
    'build-macos-devtool',
    'build-linux-devtool',
    'build-windows-devtool',
    'build-cef-webview-macos',
    'publish-npm',
  ];
  for (const jobName of checkoutJobs) {
    const checkoutStep = jobs[jobName].steps.find(
      (step) => step.uses === 'actions/checkout@v4.2.2'
    );
    assert.equal(
      checkoutStep.with.ref,
      '${{ needs.get-version.outputs.source_sha }}',
      `${jobName} must checkout the resolved source revision`
    );
  }

  const releaseStep = jobs['create-release'].steps.find(
    (step) => step.uses === 'ncipollo/release-action@v1'
  );
  assert.equal(releaseStep.with.commit, '${{ needs.get-version.outputs.source_sha }}');
  assert.equal(
    releaseStep.with.prerelease,
    "${{ needs.get-version.outputs.prerelease == 'true' }}"
  );
});

test('manual publishes are alpha-only while reusable publishes are stable-only', () => {
  assert.ok(workflow.on.workflow_call);
  assert.ok(workflow.on.workflow_dispatch);

  const versionScript = jobs['get-version'].steps.find(
    (step) => step.name === 'Validate and set version'
  ).run;
  assert.match(versionScript, /manually dispatched branch releases must use an alpha version/);
  assert.match(versionScript, /Changesets releases from main must use a stable version/);
});

test('Changesets creates a version PR and publishes an unpublished stable version from main', () => {
  assert.deepEqual(releaseWorkflow.on.push.branches, ['main']);

  const changesetJob = releaseWorkflow.jobs.changesets;
  const actionStep = changesetJob.steps.find(
    (step) => step.uses === 'changesets/action@v1'
  );
  assert.equal(actionStep.id, 'changesets');
  assert.equal(actionStep.with.cwd, 'src');
  assert.equal(actionStep.with.version, 'node tools/yarn.js version-packages');

  const releaseStep = changesetJob.steps.find(
    (step) => step.name === 'Select stable release'
  );
  assert.equal(
    releaseStep.env.HAS_CHANGESETS,
    '${{ steps.changesets.outputs.hasChangesets }}'
  );
  assert.match(releaseStep.run, /HAS_CHANGESETS.*!=.*false/);
  assert.match(releaseStep.run, /refs\/tags\/\$\{tag\}/);
  assert.match(releaseStep.run, /source_sha=\$\{GITHUB_SHA\}/);

  const publishJob = releaseWorkflow.jobs.publish;
  assert.equal(publishJob.uses, './.github/workflows/publish.yml');
  assert.equal(
    publishJob.with.source_ref,
    '${{ needs.changesets.outputs.source_sha }}'
  );
  assert.equal(publishJob.with.tag, '${{ needs.changesets.outputs.tag }}');
});

test('all packages published by the runtime workflow use one Changesets version', () => {
  const publishedPackageDirs = [
    'cef-webview',
    'create-lynxtron',
    'lynx-library-headers',
    'lynxtron',
    'lynxtron-builder',
    'lynxtron-dev-plugins',
    'lynxtron-rebuild',
  ];
  assert.equal(changesetConfig.changelog, '@changesets/cli/changelog');
  assert.deepEqual(changesetConfig.fixed, [[
    '@lynx-js/cef-webview',
    '@lynx-js/lynx-library-headers',
    '@lynx-js/lynxtron',
    '@lynx-js/lynxtron-builder',
    '@lynx-js/lynxtron-dev-plugins',
    '@lynx-js/lynxtron-rebuild',
    'create-lynxtron',
  ]]);
  assert.ok(workspacePackage.workspaces.includes('packages/cef-webview'));
  assert.ok(workspacePackage.workspaces.includes('packages/lynxtron-rebuild'));

  const publishedVersions = publishedPackageDirs.map((packageDir) => {
    const manifest = JSON.parse(
      fs.readFileSync(
        path.resolve(__dirname, `../../${packageDir}/package.json`),
        'utf8'
      )
    );
    return manifest.version;
  });
  assert.equal(new Set(publishedVersions).size, 1);

  const cefManifest = JSON.parse(
    fs.readFileSync(path.resolve(__dirname, '../../cef-webview/package.json'), 'utf8')
  );
  assert.equal(cefManifest.devDependencies['@lynx-js/lynxtron'], 'workspace:*');
  assert.match(
    publishPrepareSource,
    /"cef-webview"[\s\S]*?"replaces"[\s\S]*?"@lynx-js\/lynxtron"/
  );
});

test('release builds disable inspector and DevTool builds enable it', () => {
  for (const jobName of ['build-macos', 'build-linux', 'build-windows']) {
    const buildStep = jobs[jobName].steps.find((step) => step.uses && step.uses.includes('lynxtron-build'));
    assert.equal(buildStep.with['enable-inspector'], 'false');
  }
  for (const jobName of ['build-macos-devtool', 'build-linux-devtool', 'build-windows-devtool']) {
    const buildStep = jobs[jobName].steps.find((step) => step.uses && step.uses.includes('lynxtron-build'));
    assert.equal(buildStep.with['enable-inspector'], 'true');
    const uploadedPaths = jobs[jobName].steps
      .filter((step) => step.uses === 'actions/upload-artifact@v4')
      .map((step) => step.with.path)
      .join('\n');
    assert.match(uploadedPaths, /-devtool\.zip/);
  }
});

test('every supported platform and architecture publishes release and DevTool archives', () => {
  const targets = [
    { platform: 'darwin', arches: ['arm64', 'x64'], job: 'build-macos' },
    { platform: 'linux', arches: ['x64'], job: 'build-linux' },
    { platform: 'win32', arches: ['x64'], job: 'build-windows' },
  ];

  for (const { platform, arches, job } of targets) {
    for (const [jobName, suffix] of [[job, ''], [`${job}-devtool`, '-devtool']]) {
      assert.deepEqual(new Set(jobs[jobName].strategy.matrix.arch), new Set(arches));
      const runtimeUpload = jobs[jobName].steps.find(
        (step) => step.uses === 'actions/upload-artifact@v4' &&
          step.with.path.endsWith(`${suffix}.zip`) &&
          !step.with.path.includes('-symbols')
      );
      assert.ok(runtimeUpload, `${jobName} must upload its runtime archive`);
      assert.match(
        runtimeUpload.with.path,
        new RegExp(`lynxtron-v.*-${platform}-\\$\\{\\{ matrix\\.arch \\}\\}${suffix}\\.zip$`)
      );
    }
  }
});

test('Linux symbol extraction from main is retained for both runtime variants', () => {
  for (const jobName of ['build-linux', 'build-linux-devtool']) {
    const buildStep = jobs[jobName].steps.find((step) => step.uses && step.uses.includes('linux-lynxtron-build'));
    assert.equal(buildStep.with['gn-args'], 'symbol_level=1');

    const packageScript = jobs[jobName].steps.find(
      (step) => step.name.startsWith('Package Linux binary')
    ).run;
    assert.match(packageScript, /strip_binary\.py/);
    assert.match(packageScript, /Linux symbols package is empty or unexpectedly small/);
    if (jobName.endsWith('-devtool')) {
      assert.match(packageScript, /linux-\$\{\{ matrix\.arch \}\}-devtool\.zip/);
    }
  }
});

test('macOS releases publish both universal slice architectures', () => {
  for (const jobName of ['build-macos', 'build-macos-devtool']) {
    assert.deepEqual(
      new Set(jobs[jobName].strategy.matrix.arch),
      new Set(['arm64', 'x64'])
    );
    const uploadedPaths = jobs[jobName].steps
      .filter((step) => step.uses === 'actions/upload-artifact@v4')
      .map((step) => step.with.path)
      .join('\n');
    assert.match(uploadedPaths, /darwin-\$\{\{ matrix\.arch \}\}/);
  }

  const nodeHeadersStep = jobs['build-macos'].steps.find(
    (step) => step.name === 'Upload node headers'
  );
  assert.equal(nodeHeadersStep.if, "matrix.arch == 'arm64'");

  assert.deepEqual(
    new Set(jobs['build-cef-webview-macos'].strategy.matrix.arch),
    new Set(['arm64', 'x64'])
  );
});

test('macOS publish jobs preserve sibling architectures and cache CEF dependencies', () => {
  assert.equal(jobs['build-macos'].strategy['fail-fast'], false);
  assert.equal(jobs['build-cef-webview-macos'].strategy['fail-fast'], false);

  const cefJob = jobs['build-cef-webview-macos'];
  const cacheStep = cefJob.steps.find(
    (step) => step.uses === './lynxtron/.github/actions/common-deps'
  );
  assert.ok(cacheStep, 'CEF builds must restore and save the Habitat cache');
  assert.equal(cacheStep.with['run-habitat-sync'], 'false');

  const prepareStep = cefJob.steps.find((step) => step.name === 'Prepare environment');
  assert.equal(prepareStep.env.HABITAT_CONCURRENCY, 2);
  assert.match(prepareStep.run, /for attempt in 1 2 3/);
  assert.match(prepareStep.run, /10 \* \(2 \*\* \(attempt - 1\)\)/);
});

test('pull request CI builds every published architecture', () => {
  assert.deepEqual(
    new Set(ciJobs['macos-lynxtron-build'].strategy.matrix.arch),
    new Set(['arm64', 'x64'])
  );
  assert.deepEqual(ciJobs['linux-lynxtron-build'].strategy.matrix.arch, ['x64']);
  assert.deepEqual(ciJobs['windows-lynxtron-build'].strategy.matrix.arch, ['x64']);
});

test('Windows builds use and validate the pinned resource compiler', () => {
  const buildStep = windowsBuildAction.runs.steps.find(
    (step) => step.name === 'Build Lynxtron'
  );
  const buildScript = buildStep.run;
  const compilerPath = 'build\\toolchain\\win\\rc\\win\\rc.exe';

  assert.ok(windowsEnvSetupSource.includes(compilerPath));
  assert.match(windowsEnvSetupSource, /Test-Path -LiteralPath \$resourceCompiler -PathType Leaf/);
  assert.match(windowsEnvSetupSource, /\$env:PATH = "\$resourceCompilerDir;\$env:PATH"/);

  assert.ok(buildScript.includes(compilerPath));
  assert.match(buildScript, /Test-Path -LiteralPath \$resourceCompiler -PathType Leaf/);
  assert.match(buildScript, /Get-Command rc\.exe -CommandType Application -ErrorAction Stop/);
  assert.match(buildScript, /StringComparison\]::OrdinalIgnoreCase/);
  assert.ok(
    buildScript.indexOf('Get-Command rc.exe') <
      buildScript.indexOf('ninja.exe -C out\\Release lynxtron_app'),
    'resource compiler validation must run before the expensive build'
  );
});

test('npm packages are published once without a legacy dev release channel', () => {
  const publishJob = jobs['publish-npm'];
  const publishScript = publishJob.steps.find((step) => step.name === 'Publish').run;
  assert.equal((publishScript.match(/npm publish/g) || []).length, 7);
  assert.doesNotMatch(workflowSource, /npm dist-tag add|legacy dev tag/);
  assert.doesNotMatch(workflowSource, /dev_version|dev_tag|v\$\{VERSION\}-dev/);
});

test('legacy -dev release tags are rejected', () => {
  const versionScript = jobs['get-version'].steps.find(
    (step) => step.name === 'Validate and set version'
  ).run;
  assert.match(versionScript, /\*-dev\|\*-dev\.\*/);
  assert.match(versionScript, /must not include a '-dev' pre-release label/);
});
