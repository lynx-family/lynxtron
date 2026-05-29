import { once } from 'node:events';
import { createHash } from 'node:crypto';
import { existsSync } from 'node:fs';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { app, LynxWindow } from 'lynxtron';
import { connectCdp } from '../lib/cdp-client.js';
import { createCdpServer } from './cdp-server.js';

const EXIT = {
  success: 0,
  runtimeFailure: 1,
  loadFailure: 2,
  firstScreenTimeout: 3,
  lynxRuntimeError: 4,
  configError: 5,
  inputFailure: 6,
};

const traceEvents = [];

function now() {
  return new Date().toISOString();
}

function trace(type, data = {}) {
  traceEvents.push({ ts: now(), type, ...data });
}

function parseArgs(argv) {
  const options = {
    width: 390,
    height: 844,
    dpr: 3,
    timeoutMs: 10000,
  };
  for (const arg of argv) {
    if (!arg.startsWith('--headless-')) continue;
    const eq = arg.indexOf('=');
    const key = eq === -1 ? arg : arg.slice(0, eq);
    const value = eq === -1 ? '' : arg.slice(eq + 1);
    switch (key) {
      case '--headless-bundle':
        options.bundle = resolve(value);
        break;
      case '--headless-url':
        options.url = value;
        break;
      case '--headless-artifact-dir':
        options.artifactDir = resolve(value);
        break;
      case '--headless-screenshot':
        options.screenshot = resolve(value);
        break;
      case '--headless-report':
        options.report = resolve(value);
        break;
      case '--headless-trace':
        options.trace = resolve(value);
        break;
      case '--headless-replay':
        options.replay = resolve(value);
        break;
      case '--headless-replay-script':
        options.replayScript = resolve(value);
        break;
      case '--headless-runtime-binary':
        options.runtimeBinary = resolve(value);
        break;
      case '--headless-ui-dump':
        options.uiDump = resolve(value);
        break;
      case '--headless-ui-dump-after-tap':
        options.uiDumpAfterTap = resolve(value);
        break;
      case '--headless-tap-screenshot':
        options.tapScreenshot = resolve(value);
        break;
      case '--headless-smoke':
        options.smoke = value;
        break;
      case '--headless-cdp-port':
        options.cdpPort = Number(value);
        break;
      case '--headless-headed':
        options.headed = value !== 'false';
        break;
      case '--headless-slow-mo':
        options.slowMoMs = Number(value);
        break;
      case '--headless-record':
        options.record = value !== 'false';
        break;
      case '--headless-record-duration':
        options.recordDurationMs = Number(value);
        break;
      case '--headless-allow-empty-recording':
        options.allowEmptyRecording = value !== 'false';
        break;
      case '--headless-width':
        options.width = Number(value);
        break;
      case '--headless-height':
        options.height = Number(value);
        break;
      case '--headless-dpr':
        options.dpr = Number(value);
        break;
      case '--headless-timeout':
        options.timeoutMs = Number(value);
        break;
      case '--headless-data':
        options.dataPath = resolve(value);
        break;
      case '--headless-global-props':
        options.globalPropsPath = resolve(value);
        break;
      case '--headless-tap': {
        const [x, y] = value.split(',').map(Number);
        if (Number.isFinite(x) && Number.isFinite(y)) {
          options.tap = { x, y };
        }
        break;
      }
      case '--headless-tap-text':
        options.tapText = value;
        break;
    }
  }
  options.artifactDir ||= resolve(process.env.LYNXTRON_HEADLESS_ARTIFACT_DIR || 'artifacts');
  options.screenshot ||= resolve(options.artifactDir, 'screenshot.png');
  options.tapScreenshot ||= resolve(options.artifactDir, 'screenshot-after-tap.png');
  options.uiDump ||= resolve(options.artifactDir, 'ui-dump.json');
  options.uiDumpAfterTap ||= resolve(options.artifactDir, 'ui-dump-after-tap.json');
  options.report ||= resolve(options.artifactDir, 'report.json');
  options.trace ||= resolve(options.artifactDir, 'trace.jsonl');
  options.replay ||= resolve(options.artifactDir, 'replay.json');
  if (options.record && !Number.isFinite(options.recordDurationMs)) {
    options.recordDurationMs = options.timeoutMs;
  }
  return options;
}

async function readJsonFile(path) {
  if (!path) return {};
  return JSON.parse(await readFile(path, 'utf8'));
}

async function hashFile(path) {
  if (!path) return null;
  try {
    const data = await readFile(path);
    return {
      path,
      bytes: data.length,
      sha256: createHash('sha256').update(data).digest('hex'),
    };
  } catch (error) {
    return {
      path,
      missing: true,
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

async function writeArtifacts(options, report, replayManifest) {
  await mkdir(options.artifactDir, { recursive: true });
  await mkdir(dirname(options.screenshot), { recursive: true });
  await mkdir(dirname(options.tapScreenshot), { recursive: true });
  await mkdir(dirname(options.uiDump), { recursive: true });
  await mkdir(dirname(options.uiDumpAfterTap), { recursive: true });
  await mkdir(dirname(options.report), { recursive: true });
  await mkdir(dirname(options.trace), { recursive: true });
  await mkdir(dirname(options.replay), { recursive: true });
  await writeFile(options.report, JSON.stringify(report, null, 2));
  await writeFile(options.trace, traceEvents.map((event) => JSON.stringify(event)).join('\n') + '\n');
  if (replayManifest) {
    replayManifest.artifacts = {
      screenshot: await hashFile(options.screenshot),
      tapScreenshot: await hashFile(options.tapScreenshot),
      uiDump: await hashFile(options.uiDump),
      uiDumpAfterTap: await hashFile(options.uiDumpAfterTap),
      report: await hashFile(options.report),
      trace: await hashFile(options.trace),
      replay: { path: options.replay },
    };
    await writeFile(options.replay, JSON.stringify(replayManifest, null, 2));
  }
}

function waitWithTimeout(promise, timeoutMs, code) {
  let timeout;
  const timeoutPromise = new Promise((_, reject) => {
    timeout = setTimeout(() => {
      const error = new Error(code);
      error.code = code;
      reject(error);
    }, timeoutMs);
  });
  return Promise.race([promise, timeoutPromise]).finally(() => clearTimeout(timeout));
}

function waitForFirstFrame(window, timeoutMs) {
  return new Promise((resolve, reject) => {
    const startedAt = Date.now();
    const poll = () => {
      try {
        pumpHeadlessTasks(window);
        const png = window.captureHeadlessFrame();
        if (png.length > 0) {
          trace('lynx.first-frame', { bytes: png.length });
          resolve(png);
          return;
        }
      } catch (error) {
        reject(error);
        return;
      }

      if (Date.now() - startedAt >= timeoutMs) {
        const error = new Error('FIRST_FRAME_TIMEOUT');
        error.code = 'FIRST_FRAME_TIMEOUT';
        reject(error);
        return;
      }
      setTimeout(poll, 50);
    };
    poll();
  });
}

function pumpHeadlessTasks(window) {
  if (!window || typeof window.pumpHeadlessTasks !== 'function') {
    return false;
  }
  try {
    return window.pumpHeadlessTasks();
  } catch {
    return false;
  }
}

function startHeadlessPump(window) {
  const timer = setInterval(() => {
    pumpHeadlessTasks(window);
  }, 16);
  pumpHeadlessTasks(window);
  return () => clearInterval(timer);
}

function readHeadlessMetrics(window) {
  if (!window || typeof window.getHeadlessMetrics !== 'function') {
    return null;
  }
  try {
    return window.getHeadlessMetrics();
  } catch (error) {
    return {
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForFrameAfter(window, previousFrameCount, timeoutMs) {
  const startedAt = Date.now();
  while (Date.now() - startedAt < timeoutMs) {
    pumpHeadlessTasks(window);
    const metrics = readHeadlessMetrics(window);
    const framesPresented = Number(metrics?.framesPresented ?? 0);
    const png = window.captureHeadlessFrame();
    if (framesPresented > previousFrameCount && png.length > 0) {
      trace('lynx.frame-after-input', { bytes: png.length, framesPresented });
      return { png, framesPresented };
    }
    await delay(50);
  }

  const metrics = readHeadlessMetrics(window);
  return {
    png: window.captureHeadlessFrame(),
    framesPresented: Number(metrics?.framesPresented ?? 0),
  };
}

function buffersEqual(left, right) {
  return left.length === right.length && Buffer.compare(left, right) === 0;
}

function bufferFromBase64(data) {
  return Buffer.from(data || '', 'base64');
}

function cloneJson(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function shouldSlowMo(method) {
  return (
    method === 'Lynx.loadBundle' ||
    method === 'Lynx.loadURL' ||
    method === 'Page.takeScreenshot' ||
    method === 'Lynx.dumpUITree' ||
    method === 'Input.dispatchTouchEvent'
  );
}

function createCdpRecorder(cdp, options) {
  const actions = [];
  const slowMoMs = Number(options.slowMoMs || 0);
  return {
    actions,
    async send(method, params = {}) {
      actions.push({
        index: actions.length + 1,
        method,
        params: cloneJson(params),
      });
      const result = await cdp.send(method, params);
      if (slowMoMs > 0 && shouldSlowMo(method)) {
        trace('cdp.slow-mo', { method, delayMs: slowMoMs });
        await delay(slowMoMs);
      }
      return result;
    },
  };
}

function createRecorderController(state, options) {
  const recorder = {
    enabled: false,
    forwardToRuntime: true,
    captureUISnapshots: true,
    events: [],
    snapshots: [],
    errors: [],
    startedAt: null,
    stoppedAt: null,
  };
  let nextEventId = 1;
  let queue = Promise.resolve();

  const status = () => ({
    enabled: recorder.enabled,
    forwardToRuntime: recorder.forwardToRuntime,
    captureUISnapshots: recorder.captureUISnapshots,
    eventCount: recorder.events.length,
    snapshotCount: recorder.snapshots.length,
    errorCount: recorder.errors.length,
    startedAt: recorder.startedAt,
    stoppedAt: recorder.stoppedAt,
  });

  const normalize = (raw, provider = raw?.provider || 'unknown') => {
    const x = Number(raw?.x);
    const y = Number(raw?.y);
    const contentWidth = Number(raw?.contentWidth ?? options.width);
    const contentHeight = Number(raw?.contentHeight ?? options.height);
    const insideContent =
      raw?.insideContent !== false &&
      Number.isFinite(x) &&
      Number.isFinite(y) &&
      x >= 0 &&
      y >= 0 &&
      x <= contentWidth &&
      y <= contentHeight;
    return {
      id: `input-${nextEventId++}`,
      provider,
      kind: raw?.kind || 'pointer',
      type: raw?.type,
      pointerType: raw?.pointerType || raw?.deviceKind || 'mouse',
      deviceKind: raw?.deviceKind || raw?.pointerType || 'mouse',
      coordinateSpace: raw?.coordinateSpace || 'viewport',
      x,
      y,
      contentWidth,
      contentHeight,
      insideContent,
      button: raw?.button || 'none',
      buttonNumber: Number(raw?.buttonNumber ?? 0),
      buttons: Number(raw?.buttons ?? 0),
      clickCount: Number(raw?.clickCount ?? 0),
      deltaX: Number(raw?.deltaX ?? 0),
      deltaY: Number(raw?.deltaY ?? 0),
      modifiers: Array.isArray(raw?.modifiers) ? raw.modifiers : [],
      nativeTimestamp: raw?.nativeTimestamp,
      timestamp: now(),
      viewport: { width: options.width, height: options.height, dpr: options.dpr },
    };
  };

  const toTouchAction = (event) => {
    if (!event.insideContent || !Number.isFinite(event.x) || !Number.isFinite(event.y)) {
      return null;
    }
    const point = { x: event.x, y: event.y };
    if (event.type === 'touchStart' || event.type === 'mousePressed') {
      return {
        method: 'Input.dispatchTouchEvent',
        params: { type: 'touchStart', touchPoints: [point] },
      };
    }
    if (event.type === 'touchMove' || (event.type === 'mouseMoved' && event.buttons > 0)) {
      return {
        method: 'Input.dispatchTouchEvent',
        params: { type: 'touchMove', touchPoints: [point] },
      };
    }
    if (
      event.type === 'touchEnd' ||
      event.type === 'touchCancel' ||
      event.type === 'mouseReleased'
    ) {
      return {
        method: 'Input.dispatchTouchEvent',
        params: { type: event.type === 'touchCancel' ? 'touchCancel' : 'touchEnd', touchPoints: [point] },
      };
    }
    return null;
  };

  const captureSnapshot = async (event) => {
    if (!recorder.captureUISnapshots || !state.cdp) {
      return null;
    }
    if (
      event.type !== 'mouseReleased' &&
      event.type !== 'touchEnd' &&
      event.type !== 'touchCancel'
    ) {
      return null;
    }
    const dump = await state.cdp.send('Lynx.dumpUITree', {});
    const snapshot = {
      id: `snapshot-${recorder.snapshots.length + 1}`,
      afterInputEventId: event.id,
      timestamp: now(),
      nodeCount: dump.nodeCount,
      texts: dumpTexts(dump),
    };
    recorder.snapshots.push(snapshot);
    state.cdpServer?.emit('LynxRecorder.uiSnapshot', snapshot);
    trace('recorder.ui-snapshot', {
      id: snapshot.id,
      afterInputEventId: event.id,
      nodeCount: snapshot.nodeCount,
      texts: snapshot.texts.length,
    });
    return snapshot;
  };

  const observe = async (raw, meta = {}) => {
    const event = normalize(raw, meta.provider);
    if (!recorder.enabled && !meta.force) {
      return { accepted: false, reason: 'recorder-not-started', event };
    }
    recorder.events.push(event);
    state.cdpServer?.emit('LynxRecorder.inputEvent', event);
    trace('recorder.input-event', {
      id: event.id,
      provider: event.provider,
      type: event.type,
      x: event.x,
      y: event.y,
      insideContent: event.insideContent,
    });

    const action = toTouchAction(event);
    if (recorder.forwardToRuntime && action && state.cdp) {
      await state.cdp.send(action.method, action.params);
      state.cdpServer?.emit('LynxRecorder.actionRecorded', {
        inputEventId: event.id,
        action,
      });
      trace('recorder.forwarded-input', {
        inputEventId: event.id,
        method: action.method,
        type: action.params.type,
      });
    }
    await captureSnapshot(event);
    return { accepted: true, event, action };
  };

  return {
    get events() {
      return recorder.events;
    },
    get snapshots() {
      return recorder.snapshots;
    },
    get errors() {
      return recorder.errors;
    },
    status,
    start(params = {}) {
      recorder.enabled = true;
      recorder.forwardToRuntime = params.forwardToRuntime !== false;
      recorder.captureUISnapshots = params.captureUISnapshots !== false;
      recorder.startedAt = now();
      recorder.stoppedAt = null;
      state.cdpServer?.emit('LynxRecorder.recordingStarted', status());
      trace('recorder.start', status());
      return status();
    },
    async stop(reason = 'requested') {
      recorder.enabled = false;
      await queue;
      recorder.stoppedAt = now();
      const result = { ...status(), reason };
      state.cdpServer?.emit('LynxRecorder.recordingStopped', result);
      trace('recorder.stop', result);
      return result;
    },
    enqueue(raw, meta = {}) {
      queue = queue
        .then(() => observe(raw, meta))
        .catch((error) => {
          const details = {
            message: error instanceof Error ? error.message : String(error),
            code: error?.code,
          };
          recorder.errors.push(details);
          trace('recorder.error', details);
          state.cdpServer?.emit('LynxRecorder.recordingError', details);
          return { accepted: false, error: details };
        });
      return queue;
    },
    async waitForIdle() {
      await queue;
    },
  };
}

function dumpTexts(dump) {
  if (Array.isArray(dump?.texts)) {
    return dump.texts;
  }
  const texts = [];
  const visit = (node) => {
    if (!node || typeof node !== 'object') return;
    if (typeof node.text === 'string' && node.text.length > 0) {
      texts.push(node.text);
    }
    for (const child of node.children || []) {
      visit(child);
    }
  };
  visit(dump?.root);
  return texts;
}

function assertTexts(dump, expected, code) {
  const texts = dumpTexts(dump);
  const missing = expected.filter((text) => !texts.includes(text));
  if (missing.length > 0) {
    const error = new Error(`${code}: missing ${missing.join(', ')}`);
    error.code = code;
    error.exitCode = EXIT.inputFailure;
    error.details = { missing, texts };
    throw error;
  }
}

function hasUsableBox(node) {
  const box = node?.box;
  return (
    box &&
    Number.isFinite(box.x) &&
    Number.isFinite(box.y) &&
    Number.isFinite(box.width) &&
    Number.isFinite(box.height) &&
    box.width > 0 &&
    box.height > 0
  );
}

function centerOfBox(box) {
  return {
    x: Math.round(box.x + box.width / 2),
    y: Math.round(box.y + box.height / 2),
  };
}

function findNodePathByText(dump, text) {
  const path = [];
  const visit = (node) => {
    if (!node || typeof node !== 'object') return false;
    path.push(node);
    if (typeof node.text === 'string' && node.text.includes(text)) {
      return true;
    }
    for (const child of node.children || []) {
      if (visit(child)) {
        return true;
      }
    }
    path.pop();
    return false;
  };
  if (!visit(dump?.root)) {
    return null;
  }
  return [...path];
}

function findTapPointFromText(dump, text) {
  const path = findNodePathByText(dump, text);
  if (!path) {
    const error = new Error(`Unable to find UI dump text target: ${text}`);
    error.code = 'UI_DUMP_TARGET_NOT_FOUND';
    error.exitCode = EXIT.inputFailure;
    error.details = { text, texts: dumpTexts(dump) };
    throw error;
  }

  const textNode = path.at(-1);
  const textCenter = hasUsableBox(textNode) ? centerOfBox(textNode.box) : null;
  const containingTargets = [...path]
    .reverse()
    .filter((node) => {
      if (!hasUsableBox(node) || node.type === 'text') {
        return false;
      }
      if (!textCenter) {
        return true;
      }
      return (
        textCenter.x >= node.box.x &&
        textCenter.x <= node.box.x + node.box.width &&
        textCenter.y >= node.box.y &&
        textCenter.y <= node.box.y + node.box.height
      );
    });
  const targetNode =
    containingTargets.find((node) => node.box.width >= 80 && node.box.height >= 80) ||
    containingTargets[0] ||
    textNode;

  if (!hasUsableBox(targetNode)) {
    const error = new Error(`UI dump target has no usable box: ${text}`);
    error.code = 'UI_DUMP_TARGET_BOX_UNAVAILABLE';
    error.exitCode = EXIT.inputFailure;
    error.details = { text, path };
    throw error;
  }

  return {
    ...centerOfBox(targetNode.box),
    targetText: text,
    targetNodeId: targetNode.nodeId,
    targetType: targetNode.type,
  };
}

function complexSmokeExpectations(options) {
  if (options.smoke !== 'complex') {
    return { before: [], after: [] };
  }
  return {
    before: ['Selected Basic', 'Cart total $42', 'Status idle', 'Items 3'],
    after: ['Selected Pro', 'Cart total $57', 'Status selected', 'Items 4'],
  };
}

async function sourceManifest(options) {
  if (options.bundle) {
    const file = await hashFile(options.bundle);
    return {
      type: 'bundle',
      value: options.bundle,
      bytes: file?.bytes,
      sha256: file?.sha256,
      missing: file?.missing,
    };
  }
  return {
    type: 'url',
    value: options.url,
  };
}

async function inputFileManifest(path) {
  if (!path) return null;
  const file = await hashFile(path);
  return {
    path,
    bytes: file?.bytes,
    sha256: file?.sha256,
    missing: file?.missing,
  };
}

async function createReplayManifest(options, report, actions) {
  const tap = report.input?.tap;
  const recording = report.input?.recording;
  const hasRecordedInputActions = actions.some(
    (action) => action.method === 'Input.dispatchTouchEvent'
  );
  const semanticTapText =
    options.tapText || (tap?.source === 'ui-dump' ? tap?.targetText : undefined);
  return {
    kind: 'lynxtron-headless-replay',
    schemaVersion: 1,
    createdAt: now(),
    replay: {
      defaultMode:
        options.record || options.replayScript || (recording && hasRecordedInputActions)
          ? 'recorded'
          : semanticTapText
            ? 'semantic'
            : 'exact',
    },
    runtime: {
      binary: options.runtimeBinary,
      version: report.runtimeVersion,
      nodeVersion: report.nodeVersion,
      platform: report.platform,
      backend: report.backend,
    },
    authoring: report.authoring,
    source: await sourceManifest(options),
    device: report.device,
    load: {
      timeoutMs: options.timeoutMs,
      smoke: options.smoke,
      data: await inputFileManifest(options.dataPath),
      globalProps: await inputFileManifest(options.globalPropsPath),
    },
    semantic: {
      tapText: semanticTapText,
      expectedTexts: complexSmokeExpectations(options),
    },
    actions,
    input: report.input,
    recording,
    result: {
      status: report.status,
      exitCode: report.exitCode,
      completionSignal: report.completionSignal,
      error: report.error,
    },
  };
}

function createCdpMethods(state, options) {
  return {
    async 'Lynx.loadBundle'(params) {
      if (state.loaded) {
        return { accepted: true, alreadyLoaded: true };
      }
      const bundle = params.bundle || options.bundle;
      const accepted = state.window.loadFile(bundle, {
        data: params.data || state.loadOptions.data,
        globalProps: params.globalProps || state.loadOptions.globalProps,
      });
      state.loaded = accepted;
      pumpHeadlessTasks(state.window);
      trace('lynx.load.start', { source: bundle, via: 'cdp' });
      return { accepted };
    },
    async 'Lynx.loadURL'(params) {
      if (state.loaded) {
        return { accepted: true, alreadyLoaded: true };
      }
      const url = params.url || options.url;
      const accepted = state.window.loadURL(url, {
        data: params.data || state.loadOptions.data,
        globalProps: params.globalProps || state.loadOptions.globalProps,
      });
      state.loaded = accepted;
      pumpHeadlessTasks(state.window);
      trace('lynx.load.start', { source: url, via: 'cdp' });
      return { accepted };
    },
    async 'Lynx.waitForReadyToShow'(params) {
      return waitWithTimeout(
        Promise.race([state.ready, state.failed]),
        params.timeoutMs || options.timeoutMs,
        'READY_TO_SHOW_TIMEOUT'
      );
    },
    async 'Lynx.waitForFirstScreen'(params) {
      return waitWithTimeout(
        Promise.race([state.firstScreen, state.failed]),
        params.timeoutMs || options.timeoutMs,
        'FIRST_SCREEN_TIMEOUT'
      );
    },
    async 'Lynx.waitForFrame'(params) {
      const frame = await waitWithTimeout(
        Promise.race([state.firstFrame, state.failed]),
        params.timeoutMs || options.timeoutMs,
        'FIRST_FRAME_TIMEOUT'
      );
      return { signal: frame.signal, bytes: frame.png.length };
    },
    async 'Lynx.waitForFrameAfter'(params) {
      const frame = await waitForFrameAfter(
        state.window,
        Number(params.framesPresented || 0),
        params.timeoutMs || Math.min(options.timeoutMs, 1500)
      );
      return {
        framesPresented: frame.framesPresented,
        bytes: frame.png.length,
        data: frame.png.toString('base64'),
      };
    },
    async 'Page.takeScreenshot'() {
      pumpHeadlessTasks(state.window);
      const png = state.window.captureHeadlessFrame();
      return { data: png.toString('base64'), bytes: png.length };
    },
    async 'Lynx.dumpUITree'() {
      pumpHeadlessTasks(state.window);
      if (typeof state.window.__dumpHeadlessUITreeForCDP !== 'function') {
        throw Object.assign(new Error('Headless UI dump backing is unavailable'), {
          code: 'UI_DUMP_UNAVAILABLE',
        });
      }
      return JSON.parse(state.window.__dumpHeadlessUITreeForCDP());
    },
    async 'Lynx.getHeadlessMetrics'() {
      return readHeadlessMetrics(state.window);
    },
    async 'LynxRecorder.start'(params) {
      if (!state.recorder) {
        throw Object.assign(new Error('Recorder is unavailable'), {
          code: 'RECORDER_UNAVAILABLE',
        });
      }
      return state.recorder.start(params || {});
    },
    async 'LynxRecorder.stop'(params) {
      if (!state.recorder) {
        throw Object.assign(new Error('Recorder is unavailable'), {
          code: 'RECORDER_UNAVAILABLE',
        });
      }
      return state.recorder.stop(params?.reason || 'cdp-stop');
    },
    async 'LynxRecorder.getEvents'() {
      if (!state.recorder) {
        throw Object.assign(new Error('Recorder is unavailable'), {
          code: 'RECORDER_UNAVAILABLE',
        });
      }
      await state.recorder.waitForIdle();
      return {
        status: state.recorder.status(),
        events: state.recorder.events,
        snapshots: state.recorder.snapshots,
        errors: state.recorder.errors,
      };
    },
    async 'LynxRecorder.dispatchObservedInputEvent'(params) {
      if (!state.recorder) {
        throw Object.assign(new Error('Recorder is unavailable'), {
          code: 'RECORDER_UNAVAILABLE',
        });
      }
      return state.recorder.enqueue(params, {
        provider: params?.provider || 'cdp-provider',
        force: true,
      });
    },
    async 'Input.dispatchTouchEvent'(params) {
      const type = params.type;
      const point = params.touchPoints?.[0] || state.lastTouchPoint;
      if (!point || !Number.isFinite(point.x) || !Number.isFinite(point.y)) {
        throw Object.assign(new Error('Input.dispatchTouchEvent requires a touch point'), {
          code: 'INPUT_DISPATCH_FAILED',
        });
      }
      state.lastTouchPoint = { x: point.x, y: point.y };
      const dispatch = (phase) => {
        const accepted = state.window.dispatchHeadlessPointerEvent(
          phase,
          point.x,
          point.y,
          { deviceKind: 'touch' }
        );
        trace('input.pointer', { phase, x: point.x, y: point.y, accepted, via: 'cdp' });
        if (!accepted) {
          throw Object.assign(new Error(`Headless pointer event was rejected: ${phase}`), {
            code: 'INPUT_DISPATCH_FAILED',
          });
        }
      };
      if (type === 'touchStart') {
        dispatch('add');
        dispatch('down');
      } else if (type === 'touchMove') {
        dispatch('move');
      } else if (type === 'touchEnd' || type === 'touchCancel') {
        dispatch(type === 'touchCancel' ? 'cancel' : 'up');
        dispatch('remove');
      } else {
        throw Object.assign(new Error(`Unsupported touch event type: ${type}`), {
          code: 'INPUT_DISPATCH_FAILED',
        });
      }
      await delay(16);
      return { accepted: true };
    },
  };
}

async function loadAndCaptureInitial(cdp, options) {
  const source = options.bundle || options.url;
  if (options.bundle) {
    const loadResult = await cdp.send('Lynx.loadBundle', { bundle: options.bundle });
    if (!loadResult.accepted) {
      throw Object.assign(new Error('Lynx load request was rejected'), {
        code: 'BUNDLE_LOAD_FAILED',
        exitCode: EXIT.loadFailure,
      });
    }
  } else {
    const loadResult = await cdp.send('Lynx.loadURL', { url: options.url });
    if (!loadResult.accepted) {
      throw Object.assign(new Error('Lynx load request was rejected'), {
        code: 'BUNDLE_LOAD_FAILED',
        exitCode: EXIT.loadFailure,
      });
    }
  }

  await cdp.send('Lynx.waitForReadyToShow', { timeoutMs: options.timeoutMs });
  await cdp.send('Lynx.waitForFirstScreen', { timeoutMs: options.timeoutMs });
  const frame = await cdp.send('Lynx.waitForFrame', { timeoutMs: options.timeoutMs });
  const screenshot = await cdp.send('Page.takeScreenshot', {});
  const screenshotBuffer = bufferFromBase64(screenshot.data);
  if (screenshotBuffer.length > 0) {
    await mkdir(dirname(options.screenshot), { recursive: true });
    await writeFile(options.screenshot, screenshotBuffer);
    trace('artifact.screenshot', {
      path: options.screenshot,
      bytes: screenshotBuffer.length,
      signal: `on-first-screen+${frame.signal}`,
      via: 'cdp',
    });
  } else {
    trace('artifact.screenshot.empty', { via: 'cdp' });
  }

  const uiDump = await cdp.send('Lynx.dumpUITree', {});
  await mkdir(dirname(options.uiDump), { recursive: true });
  await writeFile(options.uiDump, JSON.stringify(uiDump, null, 2));
  trace('artifact.ui-dump', {
    path: options.uiDump,
    nodeCount: uiDump.nodeCount,
    texts: dumpTexts(uiDump).length,
    via: 'cdp',
  });

  const expectations = complexSmokeExpectations(options);
  assertTexts(uiDump, expectations.before, 'UI_DUMP_BEFORE_ASSERT_FAILED');

  return { source, frame, screenshotBuffer, uiDump };
}

async function runCdpSmoke(cdp, options) {
  const source = options.bundle || options.url;
  if (options.bundle) {
    const loadResult = await cdp.send('Lynx.loadBundle', { bundle: options.bundle });
    if (!loadResult.accepted) {
      throw Object.assign(new Error('Lynx load request was rejected'), {
        code: 'BUNDLE_LOAD_FAILED',
        exitCode: EXIT.loadFailure,
      });
    }
  } else {
    const loadResult = await cdp.send('Lynx.loadURL', { url: options.url });
    if (!loadResult.accepted) {
      throw Object.assign(new Error('Lynx load request was rejected'), {
        code: 'BUNDLE_LOAD_FAILED',
        exitCode: EXIT.loadFailure,
      });
    }
  }

  await cdp.send('Lynx.waitForReadyToShow', { timeoutMs: options.timeoutMs });
  await cdp.send('Lynx.waitForFirstScreen', { timeoutMs: options.timeoutMs });
  const frame = await cdp.send('Lynx.waitForFrame', { timeoutMs: options.timeoutMs });
  const screenshot = await cdp.send('Page.takeScreenshot', {});
  const screenshotBuffer = bufferFromBase64(screenshot.data);
  if (screenshotBuffer.length > 0) {
    await mkdir(dirname(options.screenshot), { recursive: true });
    await writeFile(options.screenshot, screenshotBuffer);
    trace('artifact.screenshot', {
      path: options.screenshot,
      bytes: screenshotBuffer.length,
      signal: `on-first-screen+${frame.signal}`,
      via: 'cdp',
    });
  } else {
    trace('artifact.screenshot.empty', { via: 'cdp' });
  }

  const uiDump = await cdp.send('Lynx.dumpUITree', {});
  await mkdir(dirname(options.uiDump), { recursive: true });
  await writeFile(options.uiDump, JSON.stringify(uiDump, null, 2));
  trace('artifact.ui-dump', {
    path: options.uiDump,
    nodeCount: uiDump.nodeCount,
    texts: dumpTexts(uiDump).length,
    via: 'cdp',
  });

  const expectations = complexSmokeExpectations(options);
  assertTexts(uiDump, expectations.before, 'UI_DUMP_BEFORE_ASSERT_FAILED');

  let tapResult = null;
  const tapPoint =
    options.tap ||
    (options.tapText ? findTapPointFromText(uiDump, options.tapText) : null) ||
    (options.smoke === 'complex' ? findTapPointFromText(uiDump, 'Upgrade plan') : null);
  if (tapPoint) {
    const beforeTapMetrics = await cdp.send('Lynx.getHeadlessMetrics', {});
    const beforeTapFrameCount = Number(beforeTapMetrics?.framesPresented ?? 0);
    trace('input.tap-target', {
      x: tapPoint.x,
      y: tapPoint.y,
      source: options.tap ? 'cli' : 'ui-dump',
      targetText: tapPoint.targetText,
      targetNodeId: tapPoint.targetNodeId,
      targetType: tapPoint.targetType,
      via: 'cdp',
    });
    await cdp.send('Input.dispatchTouchEvent', {
      type: 'touchStart',
      touchPoints: [{ x: tapPoint.x, y: tapPoint.y }],
    });
    await cdp.send('Input.dispatchTouchEvent', {
      type: 'touchEnd',
      touchPoints: [{ x: tapPoint.x, y: tapPoint.y }],
    });
    const afterFrame = await cdp.send('Lynx.waitForFrameAfter', {
      framesPresented: beforeTapFrameCount,
      timeoutMs: Math.min(options.timeoutMs, 1500),
    });
    const afterScreenshotBuffer = bufferFromBase64(afterFrame.data);
    if (afterScreenshotBuffer.length > 0) {
      await mkdir(dirname(options.tapScreenshot), { recursive: true });
      await writeFile(options.tapScreenshot, afterScreenshotBuffer);
    }
    const uiDumpAfterTap = await cdp.send('Lynx.dumpUITree', {});
    await mkdir(dirname(options.uiDumpAfterTap), { recursive: true });
    await writeFile(options.uiDumpAfterTap, JSON.stringify(uiDumpAfterTap, null, 2));
    trace('artifact.ui-dump-after-tap', {
      path: options.uiDumpAfterTap,
      nodeCount: uiDumpAfterTap.nodeCount,
      texts: dumpTexts(uiDumpAfterTap).length,
      via: 'cdp',
    });
    assertTexts(uiDumpAfterTap, expectations.after, 'UI_DUMP_AFTER_ASSERT_FAILED');
    const changed =
      screenshotBuffer.length > 0 &&
      afterScreenshotBuffer.length > 0 &&
      !buffersEqual(screenshotBuffer, afterScreenshotBuffer);
    tapResult = {
      x: tapPoint.x,
      y: tapPoint.y,
      source: options.tap ? 'cli' : 'ui-dump',
      targetText: tapPoint.targetText,
      targetNodeId: tapPoint.targetNodeId,
      targetType: tapPoint.targetType,
      changed,
      framesBefore: beforeTapFrameCount,
      framesAfter: afterFrame.framesPresented,
      screenshot: options.tapScreenshot,
      uiDumpBefore: options.uiDump,
      uiDumpAfter: options.uiDumpAfterTap,
      protocol: 'cdp',
    };
    trace('input.tap', tapResult);
    if (!changed) {
      throw Object.assign(new Error('Tap did not produce a changed frame'), {
        code: 'INPUT_NO_VISUAL_CHANGE',
        exitCode: EXIT.inputFailure,
        details: tapResult,
      });
    }
  }

  return { source, completionSignal: 'on-first-screen', tapResult };
}

function recordedInputActions(manifest) {
  return (manifest.actions || [])
    .filter((action) => action?.method === 'Input.dispatchTouchEvent')
    .map((action) => ({
      method: action.method,
      params: cloneJson(action.params || {}),
    }));
}

async function runCdpRecordedReplay(cdp, options) {
  const manifest = JSON.parse(await readFile(options.replayScript, 'utf8'));
  const initial = await loadAndCaptureInitial(cdp, options);
  const actions = recordedInputActions(manifest);
  if (actions.length === 0) {
    throw Object.assign(new Error('Replay script has no recorded input actions'), {
      code: 'RECORDED_REPLAY_EMPTY',
      exitCode: EXIT.inputFailure,
    });
  }

  const beforeMetrics = await cdp.send('Lynx.getHeadlessMetrics', {});
  const beforeFrameCount = Number(beforeMetrics?.framesPresented ?? 0);
  for (const action of actions) {
    await cdp.send(action.method, action.params);
  }

  const afterFrame = await cdp.send('Lynx.waitForFrameAfter', {
    framesPresented: beforeFrameCount,
    timeoutMs: Math.min(options.timeoutMs, 1500),
  });
  const afterScreenshotBuffer = bufferFromBase64(afterFrame.data);
  if (afterScreenshotBuffer.length > 0) {
    await mkdir(dirname(options.tapScreenshot), { recursive: true });
    await writeFile(options.tapScreenshot, afterScreenshotBuffer);
  }
  const uiDumpAfterTap = await cdp.send('Lynx.dumpUITree', {});
  await mkdir(dirname(options.uiDumpAfterTap), { recursive: true });
  await writeFile(options.uiDumpAfterTap, JSON.stringify(uiDumpAfterTap, null, 2));
  trace('artifact.ui-dump-after-recorded-replay', {
    path: options.uiDumpAfterTap,
    nodeCount: uiDumpAfterTap.nodeCount,
    texts: dumpTexts(uiDumpAfterTap).length,
    via: 'cdp',
  });
  const expectations = complexSmokeExpectations(options);
  assertTexts(uiDumpAfterTap, expectations.after, 'UI_DUMP_AFTER_ASSERT_FAILED');
  const changed =
    initial.screenshotBuffer.length > 0 &&
    afterScreenshotBuffer.length > 0 &&
    !buffersEqual(initial.screenshotBuffer, afterScreenshotBuffer);
  const tapResult = {
    source: 'recorded-actions',
    actionCount: actions.length,
    changed,
    framesBefore: beforeFrameCount,
    framesAfter: afterFrame.framesPresented,
    screenshot: options.tapScreenshot,
    uiDumpBefore: options.uiDump,
    uiDumpAfter: options.uiDumpAfterTap,
    protocol: 'cdp',
  };
  trace('input.recorded-replay', tapResult);
  if (options.smoke && !changed) {
    throw Object.assign(new Error('Recorded replay did not produce a changed frame'), {
      code: 'RECORDED_REPLAY_NO_VISUAL_CHANGE',
      exitCode: EXIT.inputFailure,
      details: tapResult,
    });
  }
  return {
    source: initial.source,
    completionSignal: 'recorded-replay',
    tapResult,
  };
}

async function runCdpRecord(cdp, options) {
  const initial = await loadAndCaptureInitial(cdp, options);
  await cdp.send('LynxRecorder.start', {
    forwardToRuntime: true,
    captureUISnapshots: true,
  });
  trace('recorder.wait-for-input', {
    durationMs: options.recordDurationMs,
    provider: 'macos-lynxtron',
  });
  await delay(options.recordDurationMs);
  await cdp.send('LynxRecorder.stop', { reason: 'duration-elapsed' });
  const recording = await cdp.send('LynxRecorder.getEvents', {});

  if (!options.allowEmptyRecording && recording.events.length === 0) {
    throw Object.assign(new Error('No input events were recorded'), {
      code: 'INPUT_RECORDING_EMPTY',
      exitCode: EXIT.inputFailure,
    });
  }

  const afterScreenshot = await cdp.send('Page.takeScreenshot', {});
  const afterScreenshotBuffer = bufferFromBase64(afterScreenshot.data);
  if (afterScreenshotBuffer.length > 0) {
    await mkdir(dirname(options.tapScreenshot), { recursive: true });
    await writeFile(options.tapScreenshot, afterScreenshotBuffer);
  }
  const uiDumpAfterTap = await cdp.send('Lynx.dumpUITree', {});
  await mkdir(dirname(options.uiDumpAfterTap), { recursive: true });
  await writeFile(options.uiDumpAfterTap, JSON.stringify(uiDumpAfterTap, null, 2));
  const expectations = complexSmokeExpectations(options);
  if (recording.events.length > 0) {
    assertTexts(uiDumpAfterTap, expectations.after, 'UI_DUMP_AFTER_ASSERT_FAILED');
  }
  const changed =
    initial.screenshotBuffer.length > 0 &&
    afterScreenshotBuffer.length > 0 &&
    !buffersEqual(initial.screenshotBuffer, afterScreenshotBuffer);
  const providers = [...new Set(recording.events.map((event) => event.provider).filter(Boolean))];
  const recordingResult = {
    status: recording.status,
    eventCount: recording.events.length,
    snapshotCount: recording.snapshots.length,
    errorCount: recording.errors.length,
    events: recording.events,
    snapshots: recording.snapshots,
    errors: recording.errors,
    changed,
    screenshot: options.tapScreenshot,
    uiDumpBefore: options.uiDump,
    uiDumpAfter: options.uiDumpAfterTap,
    protocol: 'cdp',
    provider: providers.length === 0 ? 'macos-lynxtron' : providers.length === 1 ? providers[0] : 'mixed',
    providers,
  };
  trace('input.recording', {
    eventCount: recordingResult.eventCount,
    snapshotCount: recordingResult.snapshotCount,
    errorCount: recordingResult.errorCount,
    changed,
  });
  return {
    source: initial.source,
    completionSignal: 'recorded-input',
    tapResult: null,
    recordingResult,
  };
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const startedAt = Date.now();
  let exitCode = EXIT.success;
  let status = 'success';
  let error = null;
  let window = null;
  let stopPump = null;
  let cdpServer = null;
  let cdpClient = null;
  let cdpEndpoint = null;
  let completionSignal = null;
  let tapResult = null;
  let recordingResult = null;
  let replayActions = [];

  try {
    trace('harness.start');
    if (!options.bundle && !options.url) {
      throw Object.assign(new Error('Missing --headless-bundle or --headless-url'), {
        code: 'CLI_CONFIG_ERROR',
        exitCode: EXIT.configError,
      });
    }
    if (options.bundle && !existsSync(options.bundle)) {
      throw Object.assign(new Error(`Bundle not found: ${options.bundle}`), {
        code: 'BUNDLE_LOAD_FAILED',
        exitCode: EXIT.loadFailure,
      });
    }

    await waitWithTimeout(app.whenReady(), options.timeoutMs, 'APP_READY_TIMEOUT');
    trace('runtime.ready');
    window = new LynxWindow({
      width: options.width,
      height: options.height,
      title: options.headed ? 'Lynxtron Headless Authoring' : undefined,
      show: Boolean(options.headed),
      frame: Boolean(options.headed),
      skipTaskbar: !options.headed,
      headless: true,
      deviceScaleFactor: options.dpr,
    });
    trace('window.created', {
      bounds: window.getBounds(),
      contentSize: window.getContentSize(),
      headed: Boolean(options.headed),
    });
    if (options.headed) {
      trace('authoring.headed-window', {
        mode: 'windowless-renderer-visible-host',
        slowMoMs: Number(options.slowMoMs || 0),
      });
      if (typeof window.show === 'function') {
        window.show();
      }
      if (typeof window.focus === 'function') {
        window.focus();
      }
    }
    stopPump = startHeadlessPump(window);

    const data = await readJsonFile(options.dataPath);
    const globalProps = await readJsonFile(options.globalPropsPath);
    const loadOptions = { data, globalProps };

    const ready = once(window, 'ready-to-show').then(() => {
      trace('lynx.ready-to-show');
      cdpServer?.emit('Lynx.lifecycleEvent', { name: 'ready-to-show' });
      return { signal: 'ready-to-show' };
    });
    window.on('on-runtime-ready', () => {
      trace('lynx.runtime-ready', { metrics: readHeadlessMetrics(window) });
    });
    window.on('on-page-start', (_event, url) => {
      trace('lynx.on-page-start', { url, metrics: readHeadlessMetrics(window) });
    });
    window.on('on-timing-setup', (_event, timingInfo) => {
      trace('lynx.timing-setup', { bytes: String(timingInfo).length });
    });
    window.on('on-timing-update', (_event, timingInfo, updateTiming, updateFlag) => {
      trace('lynx.timing-update', {
        bytes: String(timingInfo).length,
        updateBytes: String(updateTiming).length,
        updateFlag,
      });
    });
    const firstScreen = once(window, 'on-first-screen').then(() => {
      trace('lynx.on-first-screen', { metrics: readHeadlessMetrics(window) });
      cdpServer?.emit('Lynx.lifecycleEvent', { name: 'on-first-screen' });
      return { signal: 'on-first-screen' };
    });
    const failed = once(window, 'receive-lynx-window-error').then(([_event, type, code, message]) => {
      const lynxError = new Error(String(message));
      lynxError.code = 'LYNX_RUNTIME_ERROR';
      lynxError.details = { type, code, message };
      throw lynxError;
    });
    const firstFrame = waitForFirstFrame(window, options.timeoutMs).then((png) => ({
      signal: 'software-frame',
      png,
    }));

    const cdpState = {
      window,
      loadOptions,
      ready,
      firstScreen,
      failed,
      firstFrame,
      loaded: false,
      lastTouchPoint: null,
      cdpServer: null,
      cdp: null,
      recorder: null,
    };
    const recorderController = createRecorderController(cdpState, options);
    cdpState.recorder = recorderController;
    window.on('window-input', (eventOrDetails, maybeDetails) => {
      const details =
        maybeDetails && typeof maybeDetails === 'object' ? maybeDetails : eventOrDetails;
      recorderController.enqueue(details, {
        provider: details?.provider || 'macos-lynxtron',
      });
    });

    const cdpMethods = createCdpMethods(cdpState, options);
    cdpServer = await createCdpServer({
      port: Number.isFinite(options.cdpPort) ? options.cdpPort : 0,
      methods: cdpMethods,
      trace,
    });
    cdpState.cdpServer = cdpServer;
    cdpEndpoint = cdpServer.wsEndpoint;
    trace('cdp.listening', { wsEndpoint: cdpEndpoint });
    cdpClient = await connectCdp(cdpEndpoint);
    const recorder = createCdpRecorder(cdpClient, options);
    cdpState.cdp = recorder;
    const smokeResult = options.replayScript
      ? await runCdpRecordedReplay(recorder, options)
      : options.record
        ? await runCdpRecord(cdpClient, options)
        : await runCdpSmoke(recorder, options);
    replayActions = recorder.actions;
    completionSignal = smokeResult.completionSignal;
    tapResult = smokeResult.tapResult;
    recordingResult = smokeResult.recordingResult || null;
  } catch (caught) {
    status = 'failed';
    error = {
      code: caught.code || 'PROTOCOL_INTERNAL_ERROR',
      message: caught.message || String(caught),
      details: caught.details,
    };
    exitCode = caught.exitCode ||
      (error.code === 'APP_READY_TIMEOUT' ||
      error.code === 'FIRST_FRAME_TIMEOUT' ||
      error.code === 'FIRST_SCREEN_TIMEOUT' ||
      error.code === 'READY_TO_SHOW_TIMEOUT'
        ? EXIT.firstScreenTimeout
        : error.code === 'BUNDLE_LOAD_FAILED'
          ? EXIT.loadFailure
          : error.code === 'LYNX_RUNTIME_ERROR'
            ? EXIT.lynxRuntimeError
            : EXIT.runtimeFailure);
    trace('runtime.error', error);
  }

  const report = {
    runtimeVersion: process.versions.lynxtron,
    nodeVersion: process.versions.node,
    platform: process.platform,
    backend: 'windowless-software',
    status,
    exitCode,
    source: options.bundle || options.url,
    completionSignal,
    device: {
      viewport: { width: options.width, height: options.height },
      dpr: options.dpr,
    },
    authoring: {
      headed: Boolean(options.headed),
      slowMoMs: Number(options.slowMoMs || 0),
      rendererMode: options.headed ? 'windowless-renderer-visible-host' : 'headless-windowless',
      recording: Boolean(options.record),
      recordDurationMs: Number(options.recordDurationMs || 0),
    },
    timings: {
      totalMs: Date.now() - startedAt,
    },
    artifacts: {
      screenshot: options.screenshot,
      tapScreenshot: options.tapScreenshot,
      uiDump: options.uiDump,
      uiDumpAfterTap: options.uiDumpAfterTap,
      report: options.report,
      trace: options.trace,
      replay: options.replay,
    },
    protocol: {
      cdp: {
        wsEndpoint: cdpEndpoint,
      },
    },
    input: {
      tap: tapResult,
      recording: recordingResult,
    },
    headless: readHeadlessMetrics(window),
    error,
  };

  if (stopPump) {
    stopPump();
  }
  if (cdpClient) {
    cdpClient.close();
  }
  if (cdpServer) {
    await cdpServer.close();
  }
  const replayManifest = await createReplayManifest(options, report, replayActions);
  trace('artifact.replay', {
    path: options.replay,
    actions: replayActions.length,
  });
  await writeArtifacts(options, report, replayManifest);
  if (window && !window.isDestroyed()) {
    window.close();
  }
  app.quit();
  process.exit(exitCode);
}

void main();
