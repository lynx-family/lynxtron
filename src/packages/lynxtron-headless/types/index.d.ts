export type ExitCodeName =
  | 'success'
  | 'runtimeFailure'
  | 'loadFailure'
  | 'firstScreenTimeout'
  | 'lynxRuntimeError'
  | 'configError'
  | 'inputFailure';

export const ExitCode: Readonly<Record<ExitCodeName, number>>;

export class HeadlessError extends Error {
  code: string;
  details: Record<string, unknown>;
  constructor(code: string, message: string, details?: Record<string, unknown>);
}

export interface RunBundleOptions {
  runtimeBinary?: string;
  harnessPath?: string;
  artifactDir?: string;
  screenshot?: string;
  tapScreenshot?: string;
  uiDump?: string;
  uiDumpAfterTap?: string;
  uiSnapshot?: string;
  uiSnapshotAfterTap?: string;
  report?: string;
  trace?: string;
  replay?: string;
  data?: string;
  globalProps?: string;
  width?: number;
  height?: number;
  dpr?: number;
  timeoutMs?: number;
  tap?: { x: number; y: number };
  tapText?: string;
  tapTexts?: string[];
  insertText?: string;
  insertTexts?: string[];
  dragText?: string | { text: string; dx?: number; dy?: number };
  dragTexts?: Array<string | { text: string; dx?: number; dy?: number }>;
  headed?: boolean;
  slowMo?: number;
  record?: boolean;
  recordDurationMs?: number;
  allowEmptyRecording?: boolean;
  replayScript?: string;
  smoke?: string;
  cdpPort?: number;
  mode?: 'semantic' | 'exact' | 'recorded';
  stdio?: 'pipe' | 'inherit' | 'ignore';
}

export interface RunBundleResult {
  ok: boolean;
  exitCode: number;
  signal: NodeJS.Signals | null;
  runtimeBinary: string;
  args: string[];
  artifacts: {
    artifactDir: string;
    screenshot: string;
    tapScreenshot: string;
    uiDump: string;
    uiDumpAfterTap: string;
    uiSnapshot: string;
    uiSnapshotAfterTap: string;
    report: string;
    trace: string;
    replay: string;
  };
  stdout: string;
  stderr: string;
}

export function defaultHarnessPath(): string;
export function resolveRuntimeBinary(explicitPath?: string): Promise<string>;
export function createHarnessArgs(bundleOrUrl: string, options?: RunBundleOptions): {
  args: string[];
  artifacts: RunBundleResult['artifacts'];
};
export function runBundle(bundleOrUrl: string, options?: RunBundleOptions): Promise<RunBundleResult>;
export function recordBundle(bundleOrUrl: string, options?: RunBundleOptions): Promise<RunBundleResult>;
export function runReplay(manifestPath: string, options?: RunBundleOptions): Promise<RunBundleResult>;
export function launch(options?: RunBundleOptions): Promise<{
  wsEndpoint(): string | null;
  createSession(options?: {
    device?: {
      viewport?: { width: number; height: number };
      dpr?: number;
    };
  }): Promise<{
    loadBundle(bundlePath: string, options?: RunBundleOptions): Promise<{
      waitForFirstScreen(): Promise<void>;
      screenshot(): Promise<{ path: string }>;
      report(): Promise<{ path: string; exitCode: number }>;
    }>;
    loadURL(url: string, options?: RunBundleOptions): Promise<{
      waitForFirstScreen(): Promise<void>;
      screenshot(): Promise<{ path: string }>;
      report(): Promise<{ path: string; exitCode: number }>;
    }>;
    close(): Promise<void>;
  }>;
  close(): Promise<void>;
}>;
