import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdir, readFile } from 'node:fs/promises';
import { dirname, isAbsolute, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const packageRoot = dirname(dirname(fileURLToPath(import.meta.url)));

export const ExitCode = Object.freeze({
  success: 0,
  runtimeFailure: 1,
  loadFailure: 2,
  firstScreenTimeout: 3,
  lynxRuntimeError: 4,
  configError: 5,
  inputFailure: 6,
});

export class HeadlessError extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.name = 'HeadlessError';
    this.code = code;
    this.details = details;
  }
}

function isUrl(input) {
  return /^[a-zA-Z][a-zA-Z\d+.-]*:/.test(input);
}

function absolutePath(path) {
  return isAbsolute(path) ? path : resolve(path);
}

export function defaultHarnessPath() {
  return join(packageRoot, 'harness');
}

export async function resolveRuntimeBinary(explicitPath) {
  if (explicitPath) {
    return absolutePath(explicitPath);
  }
  if (process.env.LYNXTRON_HEADLESS_RUNTIME) {
    return absolutePath(process.env.LYNXTRON_HEADLESS_RUNTIME);
  }

  try {
    const entry = require.resolve('@lynx-js/lynxtron');
    const moduleDir = dirname(entry);
    const binModule = await import(pathToFileURL(join(moduleDir, 'lynxtron_bin.js')));
    return binModule.default;
  } catch (error) {
    throw new HeadlessError(
      'RUNTIME_LAUNCH_FAILED',
      'Unable to resolve Lynxtron runtime binary. Pass --runtime or set LYNXTRON_HEADLESS_RUNTIME.',
      { cause: error instanceof Error ? error.message : String(error) }
    );
  }
}

function validateNumber(name, value) {
  if (value == null) {
    return undefined;
  }
  if (!Number.isFinite(value) || value <= 0) {
    throw new HeadlessError('CLI_CONFIG_ERROR', `${name} must be a positive number`);
  }
  return value;
}

function validateNonNegativeNumber(name, value) {
  if (value == null) {
    return undefined;
  }
  if (!Number.isFinite(value) || value < 0) {
    throw new HeadlessError('CLI_CONFIG_ERROR', `${name} must be a non-negative number`);
  }
  return value;
}

function artifactPaths(options) {
  const artifactDir = absolutePath(options.artifactDir || 'artifacts');
  return {
    artifactDir,
    screenshot: absolutePath(options.screenshot || join(artifactDir, 'screenshot.png')),
    tapScreenshot: absolutePath(
      options.tapScreenshot || join(artifactDir, 'screenshot-after-tap.png')
    ),
    uiDump: absolutePath(options.uiDump || join(artifactDir, 'ui-dump.json')),
    uiDumpAfterTap: absolutePath(
      options.uiDumpAfterTap || join(artifactDir, 'ui-dump-after-tap.json')
    ),
    uiSnapshot: absolutePath(options.uiSnapshot || join(artifactDir, 'ui-snapshot.json')),
    uiSnapshotAfterTap: absolutePath(
      options.uiSnapshotAfterTap || join(artifactDir, 'ui-snapshot-after-tap.json')
    ),
    report: absolutePath(options.report || join(artifactDir, 'report.json')),
    trace: absolutePath(options.trace || join(artifactDir, 'trace.jsonl')),
    replay: absolutePath(options.replay || join(artifactDir, 'replay.json')),
  };
}

export function createHarnessArgs(bundleOrUrl, options = {}) {
  const artifacts = artifactPaths(options);
  const sourceArg = isUrl(bundleOrUrl)
    ? `--headless-url=${bundleOrUrl}`
    : `--headless-bundle=${absolutePath(bundleOrUrl)}`;
  const args = [
    options.harnessPath || defaultHarnessPath(),
    sourceArg,
    `--headless-artifact-dir=${artifacts.artifactDir}`,
    `--headless-screenshot=${artifacts.screenshot}`,
    `--headless-tap-screenshot=${artifacts.tapScreenshot}`,
    `--headless-ui-dump=${artifacts.uiDump}`,
    `--headless-ui-dump-after-tap=${artifacts.uiDumpAfterTap}`,
    `--headless-ui-snapshot=${artifacts.uiSnapshot}`,
    `--headless-ui-snapshot-after-tap=${artifacts.uiSnapshotAfterTap}`,
    `--headless-report=${artifacts.report}`,
    `--headless-trace=${artifacts.trace}`,
    `--headless-replay=${artifacts.replay}`,
  ];

  const width = validateNumber('width', options.width);
  const height = validateNumber('height', options.height);
  const dpr = validateNumber('dpr', options.dpr);
  const timeoutMs = validateNumber('timeoutMs', options.timeoutMs);
  const slowMo = validateNonNegativeNumber('slowMo', options.slowMo);
  const recordDurationMs = validateNonNegativeNumber(
    'recordDurationMs',
    options.recordDurationMs
  );
  if (width) args.push(`--headless-width=${width}`);
  if (height) args.push(`--headless-height=${height}`);
  if (dpr) args.push(`--headless-dpr=${dpr}`);
  if (timeoutMs) args.push(`--headless-timeout=${timeoutMs}`);
  if (options.smoke) args.push(`--headless-smoke=${options.smoke}`);
  if (options.cdpPort != null) args.push(`--headless-cdp-port=${options.cdpPort}`);
  if (options.headed) args.push('--headless-headed=true');
  if (slowMo != null) args.push(`--headless-slow-mo=${slowMo}`);
  if (options.record) args.push('--headless-record=true');
  if (recordDurationMs != null) {
    args.push(`--headless-record-duration=${recordDurationMs}`);
  }
  if (options.allowEmptyRecording) {
    args.push('--headless-allow-empty-recording=true');
  }
  if (options.replayScript) {
    args.push(`--headless-replay-script=${absolutePath(options.replayScript)}`);
  }
  if (options.runtimeBinary) {
    args.push(`--headless-runtime-binary=${absolutePath(options.runtimeBinary)}`);
  }
  if (options.data) args.push(`--headless-data=${absolutePath(options.data)}`);
  if (options.globalProps) {
    args.push(`--headless-global-props=${absolutePath(options.globalProps)}`);
  }
  if (options.tap) {
    args.push(`--headless-tap=${options.tap.x},${options.tap.y}`);
  }
  if (Array.isArray(options.tapTexts)) {
    for (const tapText of options.tapTexts) {
      args.push(`--headless-tap-text=${tapText}`);
    }
  }
  if (options.tapText && !Array.isArray(options.tapTexts)) {
    args.push(`--headless-tap-text=${options.tapText}`);
  }
  if (Array.isArray(options.insertTexts)) {
    for (const text of options.insertTexts) {
      args.push(`--headless-insert-text=${text}`);
    }
  }
  if (options.insertText && !Array.isArray(options.insertTexts)) {
    args.push(`--headless-insert-text=${options.insertText}`);
  }
  if (Array.isArray(options.dragTexts)) {
    for (const dragText of options.dragTexts) {
      const text = typeof dragText === 'string' ? dragText : dragText.text;
      const dx = typeof dragText === 'string' ? 0 : Number(dragText.dx || 0);
      const dy = typeof dragText === 'string' ? -96 : Number(dragText.dy ?? -96);
      args.push(`--headless-drag-text=${text}:${dx},${dy}`);
    }
  }
  if (options.dragText && !Array.isArray(options.dragTexts)) {
    const text = typeof options.dragText === 'string' ? options.dragText : options.dragText.text;
    const dx = typeof options.dragText === 'string' ? 0 : Number(options.dragText.dx || 0);
    const dy = typeof options.dragText === 'string' ? -96 : Number(options.dragText.dy ?? -96);
    args.push(`--headless-drag-text=${text}:${dx},${dy}`);
  }
  return { args, artifacts };
}

function firstRecordedTouchPoint(manifest) {
  const action = manifest.actions?.find(
    (item) =>
      item.method === 'Input.dispatchTouchEvent' &&
      item.params?.type === 'touchStart' &&
      Array.isArray(item.params.touchPoints) &&
      Number.isFinite(item.params.touchPoints[0]?.x) &&
      Number.isFinite(item.params.touchPoints[0]?.y)
  );
  const point = action?.params?.touchPoints?.[0];
  return point ? { x: point.x, y: point.y } : undefined;
}

function replaySource(manifest) {
  const source = manifest.source;
  if (!source?.value) {
    throw new HeadlessError('CLI_CONFIG_ERROR', 'Replay manifest is missing source.value');
  }
  return source.value;
}

function replayOptions(manifest, manifestPath, options = {}) {
  const mode = options.mode || manifest.replay?.defaultMode || 'semantic';
  if (mode !== 'semantic' && mode !== 'exact' && mode !== 'recorded') {
    throw new HeadlessError(
      'CLI_CONFIG_ERROR',
      'Replay mode must be "semantic", "exact", or "recorded"'
    );
  }
  const artifactDir =
    options.artifactDir || join(dirname(absolutePath(manifestPath)), `replay-${mode}`);
  const base = {
    runtimeBinary: options.runtimeBinary || manifest.runtime?.binary,
    harnessPath: options.harnessPath,
    artifactDir,
    screenshot: options.screenshot,
    tapScreenshot: options.tapScreenshot,
    uiDump: options.uiDump,
    uiDumpAfterTap: options.uiDumpAfterTap,
    uiSnapshot: options.uiSnapshot,
    uiSnapshotAfterTap: options.uiSnapshotAfterTap,
    report: options.report,
    trace: options.trace,
    replay: options.replay,
    width: options.width ?? manifest.device?.viewport?.width,
    height: options.height ?? manifest.device?.viewport?.height,
    dpr: options.dpr ?? manifest.device?.dpr,
    timeoutMs: options.timeoutMs ?? manifest.load?.timeoutMs,
    data: options.data || manifest.load?.data?.path,
    globalProps: options.globalProps || manifest.load?.globalProps?.path,
    smoke: options.smoke ?? manifest.load?.smoke,
    headed: options.headed,
    slowMo: options.slowMo,
    replayScript: mode === 'recorded' ? absolutePath(manifestPath) : options.replayScript,
    stdio: options.stdio,
  };
  const manifestInsertTexts = manifest.input?.text?.sequence
    ?.map((item) => item?.text)
    .filter((text) => typeof text === 'string');
  if (Array.isArray(options.insertTexts)) {
    base.insertTexts = options.insertTexts;
  } else if (options.insertText) {
    base.insertText = options.insertText;
  } else if (manifestInsertTexts?.length > 0) {
    base.insertTexts = manifestInsertTexts;
  }

  if (mode === 'recorded') {
    // The harness will load the source and replay recorded input actions.
  } else if (mode === 'exact') {
    base.tap = options.tap || firstRecordedTouchPoint(manifest) || manifest.input?.tap;
  } else {
    const tapText = options.tapText || manifest.semantic?.tapText || manifest.input?.tap?.targetText;
    if (tapText) {
      base.tapText = tapText;
    } else {
      base.tap = options.tap || firstRecordedTouchPoint(manifest) || manifest.input?.tap;
    }
  }

  return base;
}

export async function runBundle(bundleOrUrl, options = {}) {
  if (!bundleOrUrl) {
    throw new HeadlessError('CLI_CONFIG_ERROR', 'bundleOrUrl is required');
  }
  if (!isUrl(bundleOrUrl) && !existsSync(absolutePath(bundleOrUrl))) {
    throw new HeadlessError('BUNDLE_LOAD_FAILED', `Bundle not found: ${bundleOrUrl}`);
  }

  const runtimeBinary = await resolveRuntimeBinary(options.runtimeBinary);
  const { args, artifacts } = createHarnessArgs(bundleOrUrl, { ...options, runtimeBinary });
  await mkdir(artifacts.artifactDir, { recursive: true });

  return new Promise((resolvePromise, reject) => {
    const child = spawn(runtimeBinary, args, {
      stdio: options.stdio || 'pipe',
      env: {
        ...process.env,
        LYNXTRON_HEADLESS_ARTIFACT_DIR: artifacts.artifactDir,
      },
    });

    let stdout = '';
    let stderr = '';
    if (child.stdout) {
      child.stdout.on('data', (chunk) => {
        stdout += chunk.toString();
      });
    }
    if (child.stderr) {
      child.stderr.on('data', (chunk) => {
        stderr += chunk.toString();
      });
    }
    child.on('error', (error) => {
      reject(new HeadlessError('RUNTIME_LAUNCH_FAILED', error.message));
    });
    child.on('close', (code, signal) => {
      resolvePromise({
        ok: code === ExitCode.success,
        exitCode: code == null ? ExitCode.runtimeFailure : code,
        signal,
        runtimeBinary,
        args,
        artifacts,
        stdout,
        stderr,
      });
    });
  });
}

export function recordBundle(bundleOrUrl, options = {}) {
  return runBundle(bundleOrUrl, {
    ...options,
    headed: options.headed ?? true,
    record: true,
  });
}

export async function runReplay(manifestPath, options = {}) {
  if (!manifestPath) {
    throw new HeadlessError('CLI_CONFIG_ERROR', 'manifestPath is required');
  }
  const absoluteManifestPath = absolutePath(manifestPath);
  const manifest = JSON.parse(await readFile(absoluteManifestPath, 'utf8'));
  if (manifest.kind !== 'lynxtron-headless-replay' || manifest.schemaVersion !== 1) {
    throw new HeadlessError('CLI_CONFIG_ERROR', 'Unsupported replay manifest');
  }
  const source = replaySource(manifest);
  return runBundle(source, replayOptions(manifest, absoluteManifestPath, options));
}

export async function launch(options = {}) {
  const runtimeBinary = await resolveRuntimeBinary(options.runtimeBinary);
  return new Runtime({ ...options, runtimeBinary });
}

class Runtime {
  constructor(options) {
    this.options = options;
  }

  wsEndpoint() {
    return null;
  }

  async createSession(options = {}) {
    return new Session(this, options);
  }

  async close() {}
}

class Session {
  constructor(runtime, options) {
    this.runtime = runtime;
    this.options = options;
  }

  async loadBundle(bundlePath, options = {}) {
    const result = await runBundle(bundlePath, {
      ...this.runtime.options,
      ...options,
      runtimeBinary: this.runtime.options.runtimeBinary,
      width: this.options.device?.viewport?.width ?? options.width,
      height: this.options.device?.viewport?.height ?? options.height,
      dpr: this.options.device?.dpr ?? options.dpr,
    });
    return new Page(result);
  }

  async loadURL(url, options = {}) {
    const result = await runBundle(url, {
      ...this.runtime.options,
      ...options,
      runtimeBinary: this.runtime.options.runtimeBinary,
    });
    return new Page(result);
  }

  async close() {}
}

class Page {
  constructor(result) {
    this.result = result;
  }

  async waitForFirstScreen() {
    if (!this.result.ok) {
      throw new HeadlessError('FIRST_SCREEN_TIMEOUT', 'Headless run did not complete successfully', this.result);
    }
  }

  async screenshot() {
    return { path: this.result.artifacts.screenshot };
  }

  async report() {
    return { path: this.result.artifacts.report, exitCode: this.result.exitCode };
  }
}
