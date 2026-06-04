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
      case '--headless-ui-snapshot':
        options.uiSnapshot = resolve(value);
        break;
      case '--headless-ui-snapshot-after-tap':
        options.uiSnapshotAfterTap = resolve(value);
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
          options.taps ||= [];
          options.taps.push({ x, y });
        }
        break;
      }
      case '--headless-tap-text':
        options.tapText = value;
        options.tapTexts ||= [];
        options.tapTexts.push(value);
        break;
      case '--headless-insert-text':
        options.insertText = value;
        options.insertTexts ||= [];
        options.insertTexts.push(value);
        break;
      case '--headless-press-key':
        options.pressKeys ||= [];
        options.pressKeys.push(value);
        break;
      case '--headless-drag': {
        const separator = value.lastIndexOf(':');
        const rawStart = separator === -1 ? value : value.slice(0, separator);
        const rawEnd = separator === -1 ? '' : value.slice(separator + 1);
        const [startX, startY] = rawStart.split(',').map(Number);
        const [endX, endY] = rawEnd.split(',').map(Number);
        if (
          Number.isFinite(startX) &&
          Number.isFinite(startY) &&
          Number.isFinite(endX) &&
          Number.isFinite(endY)
        ) {
          options.drags ||= [];
          options.drags.push({
            start: { x: startX, y: startY },
            end: { x: endX, y: endY },
          });
        }
        break;
      }
      case '--headless-drag-text': {
        const separator = value.lastIndexOf(':');
        const text = separator === -1 ? value : value.slice(0, separator);
        const rawDelta = separator === -1 ? '' : value.slice(separator + 1);
        const [dx, dy] = rawDelta.split(',').map(Number);
        options.dragTexts ||= [];
        options.dragTexts.push({
          text,
          dx: Number.isFinite(dx) ? dx : 0,
          dy: Number.isFinite(dy) ? dy : -96,
        });
        break;
      }
    }
  }
  options.artifactDir ||= resolve(process.env.LYNXTRON_HEADLESS_ARTIFACT_DIR || 'artifacts');
  options.screenshot ||= resolve(options.artifactDir, 'screenshot.png');
  options.tapScreenshot ||= resolve(options.artifactDir, 'screenshot-after-tap.png');
  options.uiDump ||= resolve(options.artifactDir, 'ui-dump.json');
  options.uiDumpAfterTap ||= resolve(options.artifactDir, 'ui-dump-after-tap.json');
  options.uiSnapshot ||= resolve(options.artifactDir, 'ui-snapshot.json');
  options.uiSnapshotAfterTap ||= resolve(options.artifactDir, 'ui-snapshot-after-tap.json');
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
  await mkdir(dirname(options.uiSnapshot), { recursive: true });
  await mkdir(dirname(options.uiSnapshotAfterTap), { recursive: true });
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
      uiSnapshot: await hashFile(options.uiSnapshot),
      uiSnapshotAfterTap: await hashFile(options.uiSnapshotAfterTap),
      report: await hashFile(options.report),
      trace: await hashFile(options.trace),
      replay: { path: options.replay },
    };
    await writeFile(options.replay, JSON.stringify(replayManifest, null, 2));
  }
}

async function writeJsonArtifact(path, value, eventType, data = {}) {
  await mkdir(dirname(path), { recursive: true });
  await writeFile(path, JSON.stringify(value, null, 2));
  trace(eventType, {
    path,
    via: 'cdp',
    ...data,
  });
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
    method === 'LynxSnapshot.capture' ||
    method === 'LynxInput.scrollIntoView' ||
    method === 'Input.dispatchTouchEvent' ||
    method === 'Input.insertText'
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
    const captured = await state.cdp.send('LynxSnapshot.capture', {});
    const snapshot = {
      id: `snapshot-${recorder.snapshots.length + 1}`,
      afterInputEventId: event.id,
      timestamp: now(),
      ...captured,
    };
    recorder.snapshots.push(snapshot);
    state.cdpServer?.emit('LynxRecorder.uiSnapshot', snapshot);
    trace('recorder.ui-snapshot', {
      id: snapshot.id,
      afterInputEventId: event.id,
      nodeCount: snapshot.nodeCount,
      visibleTextCount: snapshot.visualHealth?.visibleTextCount,
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

function normalizeBox(box) {
  if (!box || typeof box !== 'object') {
    return null;
  }
  const x = Number(box.x);
  const y = Number(box.y);
  const width = Number(box.width);
  const height = Number(box.height);
  if (
    !Number.isFinite(x) ||
    !Number.isFinite(y) ||
    !Number.isFinite(width) ||
    !Number.isFinite(height)
  ) {
    return null;
  }
  return { x, y, width, height };
}

function intersectsViewport(box, viewport) {
  return (
    box.x < viewport.width &&
    box.x + box.width > 0 &&
    box.y < viewport.height &&
    box.y + box.height > 0
  );
}

function clipBoxToViewport(box, viewport) {
  const x = Math.max(0, box.x);
  const y = Math.max(0, box.y);
  const right = Math.min(viewport.width, box.x + box.width);
  const bottom = Math.min(viewport.height, box.y + box.height);
  return {
    x,
    y,
    width: Math.max(0, right - x),
    height: Math.max(0, bottom - y),
  };
}

function textContentOf(node) {
  if (!node || typeof node !== 'object') {
    return [];
  }
  const texts = [];
  const visit = (item) => {
    if (!item || typeof item !== 'object') return;
    if (typeof item.text === 'string' && item.text.length > 0) {
      texts.push(item.text);
    }
    for (const child of item.children || []) {
      visit(child);
    }
  };
  visit(node);
  return texts;
}

function flattenDumpNodes(dump) {
  const nodes = [];
  const visit = (node, depth = 0, parentId = null) => {
    if (!node || typeof node !== 'object') return;
    nodes.push({ node, depth, parentId });
    for (const child of node.children || []) {
      visit(child, depth + 1, node.nodeId ?? null);
    }
  };
  visit(dump?.root);
  return nodes;
}

function boxOverlapArea(left, right) {
  const x = Math.max(left.x, right.x);
  const y = Math.max(left.y, right.y);
  const width = Math.min(left.x + left.width, right.x + right.width) - x;
  const height = Math.min(left.y + left.height, right.y + right.height) - y;
  return Math.max(0, width) * Math.max(0, height);
}

function repeatedCollectionCandidates(visibleTextRuns) {
  const buckets = new Map();
  for (const run of visibleTextRuns) {
    const prefix = run.text.replace(/\d+/g, '#').trim();
    if (!prefix || prefix === run.text || prefix.length < 4) {
      continue;
    }
    const bucket = buckets.get(prefix) || [];
    bucket.push(run);
    buckets.set(prefix, bucket);
  }
  return [...buckets.entries()]
    .filter(([, runs]) => runs.length >= 3)
    .map(([pattern, runs]) => ({
      pattern,
      count: runs.length,
      nodeIds: runs.map((run) => run.nodeId),
      sampleTexts: runs.slice(0, 5).map((run) => run.text),
    }));
}

function buildUiSnapshot(dump, options, params = {}) {
  const viewport = {
    width: Number(dump?.viewport?.width ?? options.width),
    height: Number(dump?.viewport?.height ?? options.height),
    dpr: Number(dump?.viewport?.devicePixelRatio ?? options.dpr),
  };
  const flattened = flattenDumpNodes(dump);
  const documentTextRuns = [];
  const visibleTextRuns = [];
  const visualBlocks = [];
  const actionCandidates = [];
  const scrollContainers = [];
  let zeroAreaCount = 0;
  let offscreenCount = 0;

  for (const { node, depth, parentId } of flattened) {
    const box = normalizeBox(node.box);
    const visible = node.visible !== false;
    if (!box || box.width <= 0 || box.height <= 0) {
      zeroAreaCount += 1;
      continue;
    }
    const inViewport = intersectsViewport(box, viewport);
    if (!inViewport) {
      offscreenCount += 1;
    }
    const clippedBox = clipBoxToViewport(box, viewport);
    const base = {
      nodeId: node.nodeId ?? null,
      parentId,
      type: node.type || 'unknown',
      depth,
      visible,
      box,
      viewportBox: clippedBox,
      center: centerOfBox(box),
    };
    if (visible && inViewport) {
      visualBlocks.push({
        ...base,
        text: typeof node.text === 'string' ? node.text : undefined,
      });
    }
    if (visible && inViewport && typeof node.text === 'string' && node.text.length > 0) {
      const textRun = {
        ...base,
        text: node.text,
        visibleInViewport: true,
      };
      visibleTextRuns.push(textRun);
      documentTextRuns.push(textRun);
    } else if (visible && typeof node.text === 'string' && node.text.length > 0) {
      documentTextRuns.push({
        ...base,
        text: node.text,
        visibleInViewport: false,
      });
    }

    const descendantTexts = textContentOf(node);
    const label = descendantTexts.join(' ').replace(/\s+/g, ' ').trim();
    const type = String(node.type || '');
    const boxArea = box.width * box.height;
    const viewportArea = viewport.width * viewport.height;
    if (
      visible &&
      inViewport &&
      type !== 'page' &&
      type !== 'text' &&
      label.length > 0 &&
      boxArea <= viewportArea * 0.8
    ) {
      actionCandidates.push({
        ...base,
        role: 'action-candidate',
        label: label.slice(0, 120),
        textCount: descendantTexts.length,
      });
    }
    if (
      type === 'scroll-view' ||
      type === 'list' ||
      type.includes('scroll') ||
      type.includes('list')
    ) {
      scrollContainers.push({
        ...base,
        label: label.slice(0, 120),
        textCount: descendantTexts.length,
      });
    }
  }

  const overlapWarnings = [];
  const textRuns = visibleTextRuns.slice(0, 200);
  for (let i = 0; i < textRuns.length; i += 1) {
    for (let j = i + 1; j < textRuns.length; j += 1) {
      const area = boxOverlapArea(textRuns[i].viewportBox, textRuns[j].viewportBox);
      const minArea = Math.min(
        textRuns[i].viewportBox.width * textRuns[i].viewportBox.height,
        textRuns[j].viewportBox.width * textRuns[j].viewportBox.height
      );
      if (minArea > 0 && area / minArea > 0.75) {
        overlapWarnings.push({
          nodeIds: [textRuns[i].nodeId, textRuns[j].nodeId],
          texts: [textRuns[i].text, textRuns[j].text],
          overlapRatio: Number((area / minArea).toFixed(3)),
        });
      }
    }
  }

  return {
    schemaVersion: 1,
    protocol: 'cdp',
    method: 'LynxSnapshot.capture',
    capturedAt: now(),
    source: {
      uiDumpMethod: 'Lynx.dumpUITree',
      screenshotMethod: params.includeScreenshot ? 'Page.takeScreenshot' : null,
      backend: dump?.backend || 'windowless-software',
    },
    viewport,
    nodeCount: Number(dump?.nodeCount ?? flattened.length),
    documentTextRuns,
    visibleTextRuns,
    visualBlocks,
    actionCandidates,
    scrollContainers,
    repeatedCollectionCandidates: repeatedCollectionCandidates(visibleTextRuns),
    visualHealth: {
      blank: visibleTextRuns.length === 0 && visualBlocks.length === 0,
      visibleTextCount: visibleTextRuns.length,
      visibleBlockCount: visualBlocks.length,
      actionCandidateCount: actionCandidates.length,
      scrollContainerCount: scrollContainers.length,
      zeroAreaCount,
      offscreenCount,
      overlapWarningCount: overlapWarnings.length,
      overlapWarnings: overlapWarnings.slice(0, 20),
    },
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
    containingTargets.find((node) => node.box.width >= 44 && node.box.height >= 32) ||
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

  let scrollAncestor = null;
  for (let index = path.length - 2; index >= 0; index -= 1) {
    const node = path[index];
    const type = String(node?.type || '');
    if (
      hasUsableBox(node) &&
      (type === 'scroll-view' || type === 'list' || type.includes('scroll') || type.includes('list'))
    ) {
      scrollAncestor = node;
      break;
    }
  }
  const targetCenter = centerOfBox(targetNode.box);
  const scrollAncestorBox = normalizeBox(scrollAncestor?.box);
  const clippedByScrollAncestor =
    scrollAncestorBox &&
    (targetCenter.x < scrollAncestorBox.x ||
      targetCenter.x > scrollAncestorBox.x + scrollAncestorBox.width ||
      targetCenter.y < scrollAncestorBox.y ||
      targetCenter.y > scrollAncestorBox.y + scrollAncestorBox.height);

  return {
    ...centerOfBox(targetNode.box),
    targetText: text,
    targetNodeId: targetNode.nodeId,
    targetType: targetNode.type,
    targetBox: normalizeBox(targetNode.box),
    scrollAncestorNodeId: scrollAncestor?.nodeId,
    scrollAncestorType: scrollAncestor?.type,
    scrollAncestorBox,
    clippedByScrollAncestor: Boolean(clippedByScrollAncestor),
  };
}

function tapPointNeedsScroll(tapPoint) {
  return Boolean(tapPoint?.clippedByScrollAncestor && tapPoint.scrollAncestorBox);
}

async function dispatchHeadlessTouch(state, phase, point, data = {}) {
  const accepted = state.window.dispatchHeadlessPointerEvent(
    phase,
    point.x,
    point.y,
    { deviceKind: 'touch' }
  );
  trace('input.pointer', { phase, x: point.x, y: point.y, accepted, via: 'cdp', ...data });
  if (!accepted) {
    throw Object.assign(new Error(`Headless pointer event was rejected: ${phase}`), {
      code: 'INPUT_DISPATCH_FAILED',
    });
  }
  await delay(16);
}

async function dispatchHeadlessDrag(state, from, to, steps = 8, data = {}) {
  await dispatchHeadlessTouch(state, 'add', from, data);
  await dispatchHeadlessTouch(state, 'down', from, data);
  for (let i = 1; i <= steps; i += 1) {
    const progress = i / steps;
    await dispatchHeadlessTouch(
      state,
      'move',
      {
        x: Math.round(from.x + (to.x - from.x) * progress),
        y: Math.round(from.y + (to.y - from.y) * progress),
      },
      data
    );
  }
  await dispatchHeadlessTouch(state, 'up', to, data);
  await dispatchHeadlessTouch(state, 'remove', to, data);
}

async function dispatchHeadlessInsertText(state, text) {
  const providerMethods = [
    'dispatchHeadlessTextInput',
    'insertHeadlessText',
    'dispatchHeadlessKeyboardText',
  ];
  const providerMethod = providerMethods.find(
    (name) => typeof state.window?.[name] === 'function'
  );
  const baseResult = {
    method: 'Input.insertText',
    text,
    textLength: text.length,
    protocol: 'cdp',
    provider: providerMethod || 'unavailable',
  };

  if (!providerMethod) {
    return {
      ...baseResult,
      accepted: false,
      errorCode: 'INPUT_TEXT_UNAVAILABLE',
      errorMessage:
        'No focused-element text input provider is exposed by the current Lynxtron runtime.',
    };
  }

  try {
    const providerResult = await state.window[providerMethod](text);
    const accepted =
      providerResult === true ||
      (providerResult && typeof providerResult === 'object' && providerResult.accepted !== false);
    return {
      ...baseResult,
      accepted,
      providerResult: cloneJson(providerResult),
      errorCode: accepted ? undefined : 'INPUT_TEXT_REJECTED',
    };
  } catch (error) {
    return {
      ...baseResult,
      accepted: false,
      errorCode: error?.code || 'INPUT_TEXT_PROVIDER_FAILED',
      errorMessage: error instanceof Error ? error.message : String(error),
    };
  }
}

async function dispatchHeadlessKeyEvent(state, params = {}) {
  const providerMethod =
    typeof state.window?.dispatchHeadlessKeyEvent === 'function'
      ? 'dispatchHeadlessKeyEvent'
      : null;
  const key = String(params.key || '');
  const type = params.type || 'keyDown';
  const text = typeof params.text === 'string' ? params.text : '';
  const baseResult = {
    method: 'Input.dispatchKeyEvent',
    key,
    type,
    text,
    textLength: text.length,
    protocol: 'cdp',
    provider: providerMethod || 'unavailable',
  };

  if (!providerMethod) {
    return {
      ...baseResult,
      accepted: false,
      errorCode: 'INPUT_KEY_UNAVAILABLE',
      errorMessage:
        'No focused-element key input provider is exposed by the current Lynxtron runtime.',
    };
  }

  try {
    const providerResult = await state.window[providerMethod]({
      type,
      key,
      text,
      logical: params.logical,
      synthesized: params.synthesized !== false,
    });
    const accepted =
      providerResult === true ||
      (providerResult && typeof providerResult === 'object' && providerResult.accepted !== false);
    return {
      ...baseResult,
      accepted,
      providerResult: cloneJson(providerResult),
      errorCode: accepted ? undefined : 'INPUT_KEY_REJECTED',
    };
  } catch (error) {
    return {
      ...baseResult,
      accepted: false,
      errorCode: error?.code || 'INPUT_KEY_PROVIDER_FAILED',
      errorMessage: error instanceof Error ? error.message : String(error),
    };
  }
}

async function scrollIntoViewByText(state, options, params = {}) {
  const text = params.text;
  if (!text) {
    throw Object.assign(new Error('LynxInput.scrollIntoView requires text'), {
      code: 'INPUT_SCROLL_TARGET_REQUIRED',
    });
  }
  const maxScrolls = Number(params.maxScrolls ?? 8);
  const margin = Number(params.margin ?? 24);
  const startedAt = Date.now();
  let dump = null;
  let lastTapPoint = null;
  let scrollCount = 0;
  const attempts = [];

  for (let index = 0; index <= maxScrolls; index += 1) {
    pumpHeadlessTasks(state.window);
    dump = JSON.parse(state.window.__dumpHeadlessUITreeForCDP());
    lastTapPoint = findTapPointFromText(dump, text);
    const visible = !tapPointNeedsScroll(lastTapPoint);
    attempts.push({
      index,
      x: lastTapPoint.x,
      y: lastTapPoint.y,
      visible,
      targetNodeId: lastTapPoint.targetNodeId,
      targetType: lastTapPoint.targetType,
      scrollAncestorNodeId: lastTapPoint.scrollAncestorNodeId,
      scrollAncestorBox: lastTapPoint.scrollAncestorBox,
      clippedByScrollAncestor: lastTapPoint.clippedByScrollAncestor,
    });
    if (visible) {
      trace('input.scroll-into-view', {
        text,
        scrollCount,
        visible,
        attempts,
        via: 'cdp',
      });
      return {
        accepted: true,
        text,
        visible: true,
        scrollCount,
        durationMs: Date.now() - startedAt,
        tapPoint: lastTapPoint,
        uiDump: dump,
        attempts,
        protocol: 'cdp',
        method: 'LynxInput.scrollIntoView',
      };
    }

    if (index === maxScrolls) {
      break;
    }

    const scrollBox = lastTapPoint.scrollAncestorBox;
    const targetBelow = lastTapPoint.y > scrollBox.y + scrollBox.height - margin;
    const x = Math.max(
      scrollBox.x + margin,
      Math.min(scrollBox.x + scrollBox.width - margin, lastTapPoint.x || scrollBox.x + scrollBox.width / 2)
    );
    const from = {
      x: Math.round(x),
      y: Math.round(targetBelow ? scrollBox.y + scrollBox.height - margin : scrollBox.y + margin),
    };
    const to = {
      x: Math.round(x),
      y: Math.round(targetBelow ? scrollBox.y + margin : scrollBox.y + scrollBox.height - margin),
    };
    const beforeMetrics = readHeadlessMetrics(state.window);
    const beforeFrameCount = Number(beforeMetrics?.framesPresented ?? 0);
    await dispatchHeadlessDrag(state, from, to, Number(params.steps ?? 8), {
      gesture: 'scrollIntoView',
      text,
      scrollIndex: scrollCount + 1,
    });
    await waitForFrameAfter(
      state.window,
      beforeFrameCount,
      Number(params.timeoutMs ?? Math.min(options.timeoutMs, 800))
    );
    scrollCount += 1;
  }

  trace('input.scroll-into-view', {
    text,
    scrollCount,
    visible: false,
    attempts,
    via: 'cdp',
  });
  throw Object.assign(new Error(`Unable to scroll text into viewport: ${text}`), {
    code: 'INPUT_SCROLL_TARGET_NOT_VISIBLE',
    exitCode: EXIT.inputFailure,
    details: {
      text,
      scrollCount,
      lastTapPoint,
      attempts,
    },
  });
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
    (action) =>
      action.method === 'Input.dispatchTouchEvent' || action.method === 'Input.insertText'
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
    async 'LynxSnapshot.capture'(params) {
      pumpHeadlessTasks(state.window);
      if (typeof state.window.__dumpHeadlessUITreeForCDP !== 'function') {
        throw Object.assign(new Error('Headless UI dump backing is unavailable'), {
          code: 'UI_DUMP_UNAVAILABLE',
        });
      }
      const dump = JSON.parse(state.window.__dumpHeadlessUITreeForCDP());
      const snapshot = buildUiSnapshot(dump, options, params);
      if (params.includeScreenshot) {
        const png = state.window.captureHeadlessFrame();
        snapshot.screenshot = {
          data: png.toString('base64'),
          bytes: png.length,
          encoding: 'base64',
          mimeType: 'image/png',
        };
      }
      trace('snapshot.capture', {
        method: 'LynxSnapshot.capture',
        visibleTextCount: snapshot.visualHealth.visibleTextCount,
        actionCandidateCount: snapshot.visualHealth.actionCandidateCount,
        includeScreenshot: Boolean(params.includeScreenshot),
        via: 'cdp',
      });
      return snapshot;
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
    async 'LynxInput.scrollIntoView'(params) {
      return scrollIntoViewByText(state, options, params || {});
    },
    async 'Input.insertText'(params) {
      const text = params?.text;
      if (typeof text !== 'string') {
        throw Object.assign(new Error('Input.insertText requires text'), {
          code: 'INPUT_TEXT_INVALID_PARAMS',
          exitCode: EXIT.inputFailure,
        });
      }
      const result = await dispatchHeadlessInsertText(state, text);
      state.inputTextResults ||= [];
      state.inputTextResults.push(result);
      trace('input.text', result);
      if (!result.accepted) {
        throw Object.assign(
          new Error(result.errorMessage || `Input.insertText failed: ${result.errorCode}`),
          {
            code: result.errorCode || 'INPUT_TEXT_REJECTED',
            exitCode: EXIT.inputFailure,
            details: result,
          }
        );
      }
      return result;
    },
    async 'Input.dispatchKeyEvent'(params) {
      const key = params?.key;
      if (typeof key !== 'string' || key.length === 0) {
        throw Object.assign(new Error('Input.dispatchKeyEvent requires key'), {
          code: 'INPUT_KEY_INVALID_PARAMS',
          exitCode: EXIT.inputFailure,
        });
      }
      const result = await dispatchHeadlessKeyEvent(state, params || {});
      state.inputKeyResults ||= [];
      state.inputKeyResults.push(result);
      trace('input.key', result);
      if (!result.accepted) {
        throw Object.assign(
          new Error(result.errorMessage || `Input.dispatchKeyEvent failed: ${result.errorCode}`),
          {
            code: result.errorCode || 'INPUT_KEY_REJECTED',
            exitCode: EXIT.inputFailure,
            details: result,
          }
        );
      }
      return result;
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
      if (type === 'touchStart') {
        await dispatchHeadlessTouch(state, 'add', point);
        await dispatchHeadlessTouch(state, 'down', point);
      } else if (type === 'touchMove') {
        await dispatchHeadlessTouch(state, 'move', point);
      } else if (type === 'touchEnd' || type === 'touchCancel') {
        await dispatchHeadlessTouch(state, type === 'touchCancel' ? 'cancel' : 'up', point);
        await dispatchHeadlessTouch(state, 'remove', point);
      } else {
        throw Object.assign(new Error(`Unsupported touch event type: ${type}`), {
          code: 'INPUT_DISPATCH_FAILED',
        });
      }
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
  const uiSnapshot = await cdp.send('LynxSnapshot.capture', {});
  await writeJsonArtifact(options.uiSnapshot, uiSnapshot, 'artifact.ui-snapshot', {
    visibleTextCount: uiSnapshot.visualHealth?.visibleTextCount,
    actionCandidateCount: uiSnapshot.visualHealth?.actionCandidateCount,
  });

  const expectations = complexSmokeExpectations(options);
  assertTexts(uiDump, expectations.before, 'UI_DUMP_BEFORE_ASSERT_FAILED');

  return { source, frame, screenshotBuffer, uiDump, uiSnapshot };
}

async function runInsertTextSequence(cdp, options, currentScreenshotBuffer, currentUiDump) {
  const insertTexts = options.insertTexts?.length
    ? options.insertTexts
    : options.insertText
      ? [options.insertText]
      : [];
  const sequence = [];
  let latestScreenshotBuffer = currentScreenshotBuffer;
  let latestUiDump = currentUiDump;

  for (let index = 0; index < insertTexts.length; index += 1) {
    const text = insertTexts[index];
    const beforeMetrics = await cdp.send('Lynx.getHeadlessMetrics', {});
    const beforeFrameCount = Number(beforeMetrics?.framesPresented ?? 0);
    let providerResult;
    try {
      providerResult = await cdp.send('Input.insertText', { text });
    } catch (error) {
      const failed = {
        index: index + 1,
        method: 'Input.insertText',
        text,
        textLength: text.length,
        accepted: false,
        errorCode: error?.code || 'INPUT_TEXT_FAILED',
        errorMessage: error instanceof Error ? error.message : String(error),
        protocol: 'cdp',
      };
      trace('input.text-sequence', failed);
      throw Object.assign(new Error(failed.errorMessage), {
        code: failed.errorCode,
        exitCode: EXIT.inputFailure,
        details: failed,
      });
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
    const uiDumpAfterText = await cdp.send('Lynx.dumpUITree', {});
    await mkdir(dirname(options.uiDumpAfterTap), { recursive: true });
    await writeFile(options.uiDumpAfterTap, JSON.stringify(uiDumpAfterText, null, 2));
    trace('artifact.ui-dump-after-text', {
      path: options.uiDumpAfterTap,
      nodeCount: uiDumpAfterText.nodeCount,
      texts: dumpTexts(uiDumpAfterText).length,
      via: 'cdp',
    });
    const uiSnapshotAfterText = await cdp.send('LynxSnapshot.capture', {});
    await writeJsonArtifact(
      options.uiSnapshotAfterTap,
      uiSnapshotAfterText,
      'artifact.ui-snapshot-after-text',
      {
        visibleTextCount: uiSnapshotAfterText.visualHealth?.visibleTextCount,
        actionCandidateCount: uiSnapshotAfterText.visualHealth?.actionCandidateCount,
      }
    );
    const screenshotChanged =
      latestScreenshotBuffer.length > 0 &&
      afterScreenshotBuffer.length > 0 &&
      !buffersEqual(latestScreenshotBuffer, afterScreenshotBuffer);
    const uiDumpChanged = JSON.stringify(latestUiDump) !== JSON.stringify(uiDumpAfterText);
    const result = {
      ...providerResult,
      index: index + 1,
      changed: screenshotChanged || uiDumpChanged,
      screenshotChanged,
      uiDumpChanged,
      framesBefore: beforeFrameCount,
      framesAfter: afterFrame.framesPresented,
      screenshot: options.tapScreenshot,
      uiDumpBefore: options.uiDump,
      uiDumpAfter: options.uiDumpAfterTap,
      uiSnapshotBefore: options.uiSnapshot,
      uiSnapshotAfter: options.uiSnapshotAfterTap,
    };
    trace('input.text-sequence', result);
    sequence.push(result);
    latestScreenshotBuffer = afterScreenshotBuffer;
    latestUiDump = uiDumpAfterText;
  }

  return {
    result: sequence.length > 0 ? { sequence, last: sequence.at(-1) } : null,
    screenshotBuffer: latestScreenshotBuffer,
    uiDump: latestUiDump,
  };
}

async function runKeySequence(cdp, options, currentScreenshotBuffer, currentUiDump) {
  const pressKeys = options.pressKeys || [];
  const sequence = [];
  let latestScreenshotBuffer = currentScreenshotBuffer;
  let latestUiDump = currentUiDump;

  const parseKeySpec = (rawKey) => {
    const parts = String(rawKey).split(':');
    const [key, type, logical, text] = parts;
    const parsed = {
      key: key || String(rawKey),
      type: type || 'keyDown',
    };
    const logicalNumber = Number(logical);
    if (Number.isFinite(logicalNumber) && logical !== '') {
      parsed.logical = logicalNumber;
    }
    if (text != null) {
      parsed.text = text;
    }
    return parsed;
  };

  for (let index = 0; index < pressKeys.length; index += 1) {
    const keySpec = parseKeySpec(pressKeys[index]);
    const beforeMetrics = await cdp.send('Lynx.getHeadlessMetrics', {});
    const beforeFrameCount = Number(beforeMetrics?.framesPresented ?? 0);
    let providerResult;
    try {
      providerResult = await cdp.send('Input.dispatchKeyEvent', {
        type: keySpec.type,
        key: keySpec.key,
        logical: keySpec.logical,
        text: keySpec.text,
        synthesized: true,
      });
    } catch (error) {
      const failed = {
        index: index + 1,
        method: 'Input.dispatchKeyEvent',
        key: keySpec.key,
        type: keySpec.type,
        logical: keySpec.logical,
        accepted: false,
        errorCode: error?.code || 'INPUT_KEY_FAILED',
        errorMessage: error instanceof Error ? error.message : String(error),
        protocol: 'cdp',
      };
      trace('input.key-sequence', failed);
      throw Object.assign(new Error(failed.errorMessage), {
        code: failed.errorCode,
        exitCode: EXIT.inputFailure,
        details: failed,
      });
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
    const uiDumpAfterKey = await cdp.send('Lynx.dumpUITree', {});
    await mkdir(dirname(options.uiDumpAfterTap), { recursive: true });
    await writeFile(options.uiDumpAfterTap, JSON.stringify(uiDumpAfterKey, null, 2));
    trace('artifact.ui-dump-after-key', {
      path: options.uiDumpAfterTap,
      nodeCount: uiDumpAfterKey.nodeCount,
      texts: dumpTexts(uiDumpAfterKey).length,
      via: 'cdp',
    });
    const uiSnapshotAfterKey = await cdp.send('LynxSnapshot.capture', {});
    await writeJsonArtifact(
      options.uiSnapshotAfterTap,
      uiSnapshotAfterKey,
      'artifact.ui-snapshot-after-key',
      {
        visibleTextCount: uiSnapshotAfterKey.visualHealth?.visibleTextCount,
        actionCandidateCount: uiSnapshotAfterKey.visualHealth?.actionCandidateCount,
      }
    );
    const screenshotChanged =
      latestScreenshotBuffer.length > 0 &&
      afterScreenshotBuffer.length > 0 &&
      !buffersEqual(latestScreenshotBuffer, afterScreenshotBuffer);
    const uiDumpChanged = JSON.stringify(latestUiDump) !== JSON.stringify(uiDumpAfterKey);
    const result = {
      ...providerResult,
      index: index + 1,
      changed: screenshotChanged || uiDumpChanged,
      screenshotChanged,
      uiDumpChanged,
      framesBefore: beforeFrameCount,
      framesAfter: afterFrame.framesPresented,
      screenshot: options.tapScreenshot,
      uiDumpBefore: options.uiDump,
      uiDumpAfter: options.uiDumpAfterTap,
      uiSnapshotBefore: options.uiSnapshot,
      uiSnapshotAfter: options.uiSnapshotAfterTap,
    };
    trace('input.key-sequence', result);
    sequence.push(result);
    latestScreenshotBuffer = afterScreenshotBuffer;
    latestUiDump = uiDumpAfterKey;
  }

  return {
    result: sequence.length > 0 ? { sequence, last: sequence.at(-1) } : null,
    screenshotBuffer: latestScreenshotBuffer,
    uiDump: latestUiDump,
  };
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
  const uiSnapshot = await cdp.send('LynxSnapshot.capture', {});
  await writeJsonArtifact(options.uiSnapshot, uiSnapshot, 'artifact.ui-snapshot', {
    visibleTextCount: uiSnapshot.visualHealth?.visibleTextCount,
    actionCandidateCount: uiSnapshot.visualHealth?.actionCandidateCount,
  });

  const expectations = complexSmokeExpectations(options);
  assertTexts(uiDump, expectations.before, 'UI_DUMP_BEFORE_ASSERT_FAILED');

  let currentScreenshotBuffer = screenshotBuffer;
  let currentUiDump = uiDump;
  let tapResult = null;
  const tapResults = [];
  const tapSequence = [];
  const taps = options.taps?.length ? options.taps : options.tap ? [options.tap] : [];
  for (const tap of taps) {
    tapSequence.push({ point: tap, source: 'cli' });
  }
  const tapTexts = options.tapTexts?.length ? options.tapTexts : options.tapText ? [options.tapText] : [];
  for (const text of tapTexts) {
    tapSequence.push({ text, source: 'ui-dump' });
  }
  if (tapSequence.length === 0 && options.smoke === 'complex') {
    tapSequence.push({ text: 'Upgrade plan', source: 'ui-dump' });
  }

  for (let index = 0; index < tapSequence.length; index += 1) {
    const tapSpec = tapSequence[index];
    let scrollResult = null;
    let tapPoint = tapSpec.point || findTapPointFromText(currentUiDump, tapSpec.text);
    if (tapSpec.text && tapPointNeedsScroll(tapPoint)) {
      scrollResult = await cdp.send('LynxInput.scrollIntoView', {
        text: tapSpec.text,
        timeoutMs: Math.min(options.timeoutMs, 1000),
      });
      currentUiDump = scrollResult.uiDump;
      tapPoint = scrollResult.tapPoint || findTapPointFromText(currentUiDump, tapSpec.text);
    }
    const beforeTapMetrics = await cdp.send('Lynx.getHeadlessMetrics', {});
    const beforeTapFrameCount = Number(beforeTapMetrics?.framesPresented ?? 0);
    trace('input.tap-target', {
      index: index + 1,
      x: tapPoint.x,
      y: tapPoint.y,
      source: tapSpec.source,
      targetText: tapPoint.targetText,
      targetNodeId: tapPoint.targetNodeId,
      targetType: tapPoint.targetType,
      scrolled: Boolean(scrollResult),
      scrollCount: scrollResult?.scrollCount,
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
    const uiSnapshotAfterTap = await cdp.send('LynxSnapshot.capture', {});
    await writeJsonArtifact(
      options.uiSnapshotAfterTap,
      uiSnapshotAfterTap,
      'artifact.ui-snapshot-after-tap',
      {
        visibleTextCount: uiSnapshotAfterTap.visualHealth?.visibleTextCount,
        actionCandidateCount: uiSnapshotAfterTap.visualHealth?.actionCandidateCount,
      }
    );
    const screenshotChanged =
      currentScreenshotBuffer.length > 0 &&
      afterScreenshotBuffer.length > 0 &&
      !buffersEqual(currentScreenshotBuffer, afterScreenshotBuffer);
    const uiDumpChanged = JSON.stringify(currentUiDump) !== JSON.stringify(uiDumpAfterTap);
    const changed = screenshotChanged || uiDumpChanged;
    tapResult = {
      index: index + 1,
      x: tapPoint.x,
      y: tapPoint.y,
      source: tapSpec.source,
      targetText: tapPoint.targetText,
      targetNodeId: tapPoint.targetNodeId,
      targetType: tapPoint.targetType,
      scrolled: Boolean(scrollResult),
      scrollCount: scrollResult?.scrollCount,
      changed,
      screenshotChanged,
      uiDumpChanged,
      framesBefore: beforeTapFrameCount,
      framesAfter: afterFrame.framesPresented,
      screenshot: options.tapScreenshot,
      uiDumpBefore: options.uiDump,
      uiDumpAfter: options.uiDumpAfterTap,
      uiSnapshotBefore: options.uiSnapshot,
      uiSnapshotAfter: options.uiSnapshotAfterTap,
      protocol: 'cdp',
    };
    trace('input.tap', tapResult);
    tapResults.push(tapResult);
    if (!changed) {
      throw Object.assign(new Error('Tap did not produce a changed frame'), {
        code: 'INPUT_NO_VISUAL_CHANGE',
        exitCode: EXIT.inputFailure,
        details: tapResult,
      });
    }
    currentScreenshotBuffer = afterScreenshotBuffer;
    currentUiDump = uiDumpAfterTap;
  }
  if (tapResults.length > 1 && tapResult) {
    tapResult = {
      ...tapResult,
      sequence: tapResults.map((tap) => ({ ...tap })),
    };
  }
  if (tapResults.length > 0) {
    assertTexts(currentUiDump, expectations.after, 'UI_DUMP_AFTER_ASSERT_FAILED');
  }

  let textInputResult = null;
  if (options.insertText || options.insertTexts?.length) {
    const textSequence = await runInsertTextSequence(
      cdp,
      options,
      currentScreenshotBuffer,
      currentUiDump
    );
    textInputResult = textSequence.result;
    currentScreenshotBuffer = textSequence.screenshotBuffer;
    currentUiDump = textSequence.uiDump;
  }

  let keyInputResult = null;
  if (options.pressKeys?.length) {
    const keySequence = await runKeySequence(
      cdp,
      options,
      currentScreenshotBuffer,
      currentUiDump
    );
    keyInputResult = keySequence.result;
    currentScreenshotBuffer = keySequence.screenshotBuffer;
    currentUiDump = keySequence.uiDump;
  }

  const dragResults = [];
  const dragSequence = [];
  const drags = options.drags || [];
  for (const drag of drags) {
    dragSequence.push({
      source: 'cli',
      start: drag.start,
      end: drag.end,
    });
  }
  const dragTexts = options.dragTexts || [];
  for (const dragText of dragTexts) {
    dragSequence.push({
      ...dragText,
      source: 'ui-dump',
    });
  }
  for (let index = 0; index < dragSequence.length; index += 1) {
    const dragSpec = dragSequence[index];
    let scrollResult = null;
    let dragPoint = null;
    if (dragSpec.source === 'ui-dump') {
      dragPoint = findTapPointFromText(currentUiDump, dragSpec.text);
      if (tapPointNeedsScroll(dragPoint)) {
        scrollResult = await cdp.send('LynxInput.scrollIntoView', {
          text: dragSpec.text,
          timeoutMs: Math.min(options.timeoutMs, 1000),
        });
        currentUiDump = scrollResult.uiDump;
        dragPoint = scrollResult.tapPoint || findTapPointFromText(currentUiDump, dragSpec.text);
      }
    }
    const beforeDragMetrics = await cdp.send('Lynx.getHeadlessMetrics', {});
    const beforeDragFrameCount = Number(beforeDragMetrics?.framesPresented ?? 0);
    const start =
      dragSpec.source === 'cli'
        ? { x: Math.round(dragSpec.start.x), y: Math.round(dragSpec.start.y) }
        : { x: dragPoint.x, y: dragPoint.y };
    const end =
      dragSpec.source === 'cli'
        ? { x: Math.round(dragSpec.end.x), y: Math.round(dragSpec.end.y) }
        : {
            x: Math.round(dragPoint.x + Number(dragSpec.dx || 0)),
            y: Math.round(dragPoint.y + Number(dragSpec.dy || 0)),
          };
    trace('input.drag-target', {
      index: index + 1,
      source: dragSpec.source,
      targetText: dragPoint?.targetText,
      targetNodeId: dragPoint?.targetNodeId,
      targetType: dragPoint?.targetType,
      start,
      end,
      scrolled: Boolean(scrollResult),
      scrollCount: scrollResult?.scrollCount,
      via: 'cdp',
    });
    await cdp.send('Input.dispatchTouchEvent', {
      type: 'touchStart',
      touchPoints: [start],
    });
    const steps = Number(dragSpec.steps || 8);
    for (let step = 1; step <= steps; step += 1) {
      const progress = step / steps;
      await cdp.send('Input.dispatchTouchEvent', {
        type: 'touchMove',
        touchPoints: [
          {
            x: Math.round(start.x + (end.x - start.x) * progress),
            y: Math.round(start.y + (end.y - start.y) * progress),
          },
        ],
      });
    }
    await cdp.send('Input.dispatchTouchEvent', {
      type: 'touchEnd',
      touchPoints: [end],
    });
    const afterFrame = await cdp.send('Lynx.waitForFrameAfter', {
      framesPresented: beforeDragFrameCount,
      timeoutMs: Math.min(options.timeoutMs, 1500),
    });
    const afterScreenshotBuffer = bufferFromBase64(afterFrame.data);
    if (afterScreenshotBuffer.length > 0) {
      await mkdir(dirname(options.tapScreenshot), { recursive: true });
      await writeFile(options.tapScreenshot, afterScreenshotBuffer);
    }
    const uiDumpAfterDrag = await cdp.send('Lynx.dumpUITree', {});
    await mkdir(dirname(options.uiDumpAfterTap), { recursive: true });
    await writeFile(options.uiDumpAfterTap, JSON.stringify(uiDumpAfterDrag, null, 2));
    const uiSnapshotAfterDrag = await cdp.send('LynxSnapshot.capture', {});
    await writeJsonArtifact(
      options.uiSnapshotAfterTap,
      uiSnapshotAfterDrag,
      'artifact.ui-snapshot-after-drag',
      {
        visibleTextCount: uiSnapshotAfterDrag.visualHealth?.visibleTextCount,
        actionCandidateCount: uiSnapshotAfterDrag.visualHealth?.actionCandidateCount,
      }
    );
    const screenshotChanged =
      currentScreenshotBuffer.length > 0 &&
      afterScreenshotBuffer.length > 0 &&
      !buffersEqual(currentScreenshotBuffer, afterScreenshotBuffer);
    const uiDumpChanged = JSON.stringify(currentUiDump) !== JSON.stringify(uiDumpAfterDrag);
    const changed = screenshotChanged || uiDumpChanged;
    const dragResult = {
      index: index + 1,
      source: dragSpec.source,
      targetText: dragPoint?.targetText,
      targetNodeId: dragPoint?.targetNodeId,
      targetType: dragPoint?.targetType,
      start,
      end,
      dx: dragSpec.dx,
      dy: dragSpec.dy,
      scrolled: Boolean(scrollResult),
      scrollCount: scrollResult?.scrollCount,
      changed,
      screenshotChanged,
      uiDumpChanged,
      framesBefore: beforeDragFrameCount,
      framesAfter: afterFrame.framesPresented,
      protocol: 'cdp',
    };
    trace('input.drag', dragResult);
    dragResults.push(dragResult);
    currentScreenshotBuffer = afterScreenshotBuffer;
    currentUiDump = uiDumpAfterDrag;
  }
  if (dragResults.length > 0) {
    tapResult = {
      ...(tapResult || {}),
      dragSequence: dragResults.map((drag) => ({ ...drag })),
    };
  }

  return {
    source,
    completionSignal: 'on-first-screen',
    tapResult,
    textInputResult,
    keyInputResult,
  };
}

function recordedInputActions(manifest) {
  return (manifest.actions || [])
    .filter(
      (action) =>
        action?.method === 'Input.dispatchTouchEvent' ||
        action?.method === 'Input.insertText' ||
        action?.method === 'Input.dispatchKeyEvent'
    )
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
  const uiSnapshotAfterTap = await cdp.send('LynxSnapshot.capture', {});
  await writeJsonArtifact(
    options.uiSnapshotAfterTap,
    uiSnapshotAfterTap,
    'artifact.ui-snapshot-after-recorded-replay',
    {
      visibleTextCount: uiSnapshotAfterTap.visualHealth?.visibleTextCount,
      actionCandidateCount: uiSnapshotAfterTap.visualHealth?.actionCandidateCount,
    }
  );
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
    uiSnapshotBefore: options.uiSnapshot,
    uiSnapshotAfter: options.uiSnapshotAfterTap,
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
  const uiSnapshotAfterTap = await cdp.send('LynxSnapshot.capture', {});
  await writeJsonArtifact(
    options.uiSnapshotAfterTap,
    uiSnapshotAfterTap,
    'artifact.ui-snapshot-after-record',
    {
      visibleTextCount: uiSnapshotAfterTap.visualHealth?.visibleTextCount,
      actionCandidateCount: uiSnapshotAfterTap.visualHealth?.actionCandidateCount,
    }
  );
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
    uiSnapshotBefore: options.uiSnapshot,
    uiSnapshotAfter: options.uiSnapshotAfterTap,
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
  let textInputResult = null;
  let keyInputResult = null;
  const inputTextResults = [];
  const inputKeyResults = [];
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
      inputTextResults,
      inputKeyResults,
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
    textInputResult = smokeResult.textInputResult || null;
    keyInputResult = smokeResult.keyInputResult || null;
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
      uiSnapshot: options.uiSnapshot,
      uiSnapshotAfterTap: options.uiSnapshotAfterTap,
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
      text:
        textInputResult ||
        (inputTextResults.length > 0
          ? { sequence: inputTextResults, last: inputTextResults.at(-1) }
          : null),
      key:
        keyInputResult ||
        (inputKeyResults.length > 0
          ? { sequence: inputKeyResults, last: inputKeyResults.at(-1) }
          : null),
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
