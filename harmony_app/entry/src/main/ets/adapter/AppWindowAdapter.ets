import Want from '@ohos.app.ability.Want';
import common from '@ohos.app.ability.common';
import { contextConstant, StartOptions } from '@kit.AbilityKit';
import bundleManager from '@ohos.bundle.bundleManager';
import { BusinessError } from '@ohos.base';
import hilog from '@ohos.hilog';
import window from '@ohos.window';
import display from '@ohos.display';
import lynxtron from 'liblynxtron_napi.so';
import { AbilityManager } from '../common/AbilityManager';
import { LynxWindowProxy } from '../common/LynxWindowProxy';
import { CreateWindowOptions, WindowBounds } from '../common/WindowTypes';
import { SubWindowAdapter } from './SubWindowAdapter';
import { WindowOpAdapter, WindowOpCallbacks } from './WindowOpAdapter';
import { reportWindowRect } from '../utils/WindowUtils';

const DOMAIN = 0x0000;
const TAG = 'AppWindowAdapter';

// startAbility() fails with 16000067 ("The StartOptions check failed") while
// the status-bar icon is not attached yet. The attach completes asynchronously
// (and its completion callback is unreliable), so probe by retrying.
const ERR_START_OPTIONS_CHECK_FAILED = 16000067;
const START_MAX_ATTEMPTS = 25;
const START_RETRY_INTERVAL_MS = 200;
const PARENT_RESOLVE_MAX_ATTEMPTS = 25;
const PARENT_RESOLVE_RETRY_INTERVAL_MS = 40;

// Legacy param used by EntryAbility to launch the initial window.
export interface CreateWindowParam {
  context: common.UIAbilityContext;
  instanceKey: string;
  abilityName: string;
  show: boolean;
  bounds?: WindowBounds;
}

export { WindowOpCallbacks };

// Centers the given bounds on the target display (the default display if
// unspecified), preserving the window size.
function centerBounds(bounds: WindowBounds, targetDisplay?: display.Display): WindowBounds {
  try {
    const disp = targetDisplay ?? display.getDefaultDisplaySync();
    return {
      left: Math.round((disp.width - bounds.width) / 2),
      top: Math.round((disp.height - bounds.height) / 2),
      width: bounds.width,
      height: bounds.height,
    };
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'centerBounds failed: %{public}s', String(e));
    return bounds;
  }
}

// Returns the id of the built-in display running in MAIN source mode, or 0
// if no usable internal display is found.
function getInternalDisplayId(): number {
  // The internal/built-in display must have sourceMode MAIN. In extend mode
  // getPrimaryDisplaySync/getDefaultDisplaySync may return the external
  // monitor, which cannot host an Ability, so only accept MAIN here.
  try {
    const primary = display.getPrimaryDisplaySync();
    if (primary.sourceMode === display.DisplaySourceMode.MAIN) {
      return primary.id;
    }
    hilog.warn(DOMAIN, TAG,
      'getPrimaryDisplaySync id=%{public}d sourceMode=%{public}d is not MAIN, keep searching',
      primary.id, primary.sourceMode);
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'getPrimaryDisplaySync failed: %{public}s', String(e));
  }
  try {
    const defaultDisp = display.getDefaultDisplaySync();
    if (defaultDisp.sourceMode === display.DisplaySourceMode.MAIN) {
      return defaultDisp.id;
    }
    hilog.warn(DOMAIN, TAG,
      'getDefaultDisplaySync id=%{public}d sourceMode=%{public}d is not MAIN, keep searching',
      defaultDisp.id, defaultDisp.sourceMode);
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'getDefaultDisplaySync failed: %{public}s', String(e));
  }
  return 0;
}

// Returns the display on which EntryAbility is running, or undefined if that
// display is gone or no longer usable.
function getEntryAbilityDisplayId(): number | undefined {
  const id = AppStorage.get<number>('entry_ability_display_id');
  if (id === undefined || id < 0) {
    return undefined;
  }
  try {
    const disp = display.getDisplayByIdSync(id);
    if (isUsableSourceMode(disp.sourceMode)) {
      return id;
    }
    hilog.warn(DOMAIN, TAG,
      'entry_ability_display_id=%{public}d sourceMode=%{public}d is not usable, ignore',
      id, disp.sourceMode);
  } catch (e) {
    hilog.warn(DOMAIN, TAG,
      'entry_ability_display_id=%{public}d invalid: %{public}s', id, String(e));
  }
  return undefined;
}

// Returns the display with the given id, falling back to the default display.
function getDisplayByIdOrDefault(displayId: number): display.Display | undefined {
  try {
    return display.getDisplayByIdSync(displayId);
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'getDisplayByIdSync(%{public}d) failed: %{public}s', displayId, String(e));
  }
  try {
    return display.getDefaultDisplaySync();
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'getDefaultDisplaySync fallback failed: %{public}s', String(e));
  }
  return undefined;
}

// Only MAIN and EXTEND displays can host an Ability; mirror-mode displays
// cannot and are rejected.
function isUsableSourceMode(mode: display.DisplaySourceMode | undefined): boolean {
  if (mode === undefined) {
    return false;
  }
  return mode === display.DisplaySourceMode.MAIN || mode === display.DisplaySourceMode.EXTEND;
}

// Resolves which display a new window should be created on: the requested
// display if usable, else EntryAbility's display, else the internal display.
function resolveDisplayId(requestedDisplayId?: number): number {
  if (requestedDisplayId !== undefined && requestedDisplayId >= 0) {
    try {
      const disp = display.getDisplayByIdSync(requestedDisplayId);
      const mode = disp.sourceMode;
      if (isUsableSourceMode(mode)) {
        return requestedDisplayId;
      }
      hilog.warn(DOMAIN, TAG,
        'requestedDisplayId=%{public}d sourceMode=%{public}d is not usable, fallback',
        requestedDisplayId, mode);
    } catch (e) {
      hilog.warn(DOMAIN, TAG,
        'requestedDisplayId=%{public}d is invalid: %{public}s, fallback',
        requestedDisplayId, String(e));
    }
  }
  // Prefer the display where EntryAbility is running; otherwise fall back to
  // the built-in MAIN display.
  const entryDisplayId = getEntryAbilityDisplayId();
  if (entryDisplayId !== undefined) {
    return entryDisplayId;
  }
  return getInternalDisplayId();
}

// Maps a C++ window type to the Ability that hosts it. All top-level window
// types are currently hosted by WindowAbility; subwindows never reach here.
function getAbilityNameForType(type: string): string {
  switch (type) {
    case 'main':
    default:
      return 'WindowAbility';
  }
}

export class AppWindowAdapter {
  // Called from C++ via the NAPI bridge when JS does `new BaseWindow(options)`.
  static createWindowFromCpp(options: CreateWindowOptions): void {
    const parentHarmonyId = options.parentHarmonyWindowId ?? -1;
    const hasParent = parentHarmonyId > 0 ||
      (options.parentWindowId !== undefined && options.parentWindowId > 0);
    const explicitlySubWindow = options.type === 'sub' ||
      options.type === 'panel' || options.type === 'dialog';
    let parent = parentHarmonyId > 0 ? AbilityManager.get(parentHarmonyId) : undefined;
    if (!parent && options.parentWindowId && options.parentWindowId > 0) {
      parent = AbilityManager.getByCppWindowId(options.parentWindowId);
    }
    if (hasParent && !parent) {
      const parentResolveAttempt = options.parentResolveAttempt ?? 1;
      if (parentResolveAttempt < PARENT_RESOLVE_MAX_ATTEMPTS) {
        options.parentResolveAttempt = parentResolveAttempt + 1;
        if (parentResolveAttempt === 1) {
          hilog.info(DOMAIN, TAG,
            'waiting for parent window parentWindowId=%{public}d parentHarmonyWindowId=%{public}d',
            options.parentWindowId ?? -1, parentHarmonyId);
        }
        setTimeout(() => AppWindowAdapter.createWindowFromCpp(options),
          PARENT_RESOLVE_RETRY_INTERVAL_MS);
        return;
      }
    }

    hilog.info(DOMAIN, TAG,
      'createWindowFromCpp windowId=%{public}d type=%{public}s bounds=%{public}d,%{public}d,%{public}dx%{public}d',
      options.windowId, options.type,
      options.x ?? 0, options.y ?? 0,
      options.width ?? 800, options.height ?? 600);

    // First-window binding: the EntryAbility placeholder proxy (cppWindowId === 0)
    // should consume the first 'main' window request instead of launching a new
    // WindowAbility. This avoids having both the EntryAbility window and a
    // separate WindowAbility window at startup.
    if (options.type === 'main') {
      const mainProxy = AbilityManager.getMainProxy();
      if (mainProxy) {
        AppWindowAdapter.bindToMainWindow(mainProxy, options);
        return;
      }
    }

    // Subwindows must be created via the parent's windowStage. If no parent is
    // found after retries, the request is rejected; independent Ability fallback
    // has been removed.
    if (explicitlySubWindow) {
      if (!parent) {
        hilog.error(DOMAIN, TAG,
          '%{public}s window rejected: parent not found (parentHarmonyWindowId=%{public}d, parentWindowId=%{public}d)',
          options.type, parentHarmonyId, options.parentWindowId ?? -1);
        return;
      }
      SubWindowAdapter.createSubWindow(parent, options);
      return;
    }

    if (hasParent && !parent) {
      hilog.error(DOMAIN, TAG,
        '%{public}s window rejected: parent not found (parentHarmonyWindowId=%{public}d, parentWindowId=%{public}d)',
        options.type, parentHarmonyId, options.parentWindowId ?? -1);
      return;
    }

    // Resolve the target display. Mirror-mode displays cannot host an Ability,
    // so fall back to the internal display when no valid display is requested.
    const requestedDisplayId = options.displayId;
    const resolvedDisplayId = resolveDisplayId(requestedDisplayId);
    const targetDisplay = getDisplayByIdOrDefault(resolvedDisplayId);
    hilog.info(DOMAIN, TAG,
      'createWindowFromCpp requestedDisplayId=%{public}d resolvedDisplayId=%{public}d sourceMode=%{public}d',
      requestedDisplayId ?? -1, resolvedDisplayId, targetDisplay?.sourceMode ?? -1);

    // Compute initial bounds, applying center if requested and no explicit x/y
    // were provided. Update options.x/y so that applyWindowOptions also positions
    // the window at the same place.
    let bounds: WindowBounds = {
      left: options.x ?? 100,
      top: options.y ?? 100,
      width: options.width ?? 800,
      height: options.height ?? 600,
    };
    if (options.center === true && options.hasX !== true && options.hasY !== true) {
      bounds = centerBounds(bounds, targetDisplay);
      options.x = bounds.left;
      options.y = bounds.top;
    }

    // Diagnostic logging for multi-display / mirror-mode issues.
    try {
      const defaultDisp = display.getDefaultDisplaySync();
      hilog.info(DOMAIN, TAG,
        'createWindowFromCpp default display id=%{public}d width=%{public}d height=%{public}d',
        defaultDisp.id, defaultDisp.width, defaultDisp.height);

      display.getAllDisplays()
        .then((displays) => {
          for (const d of displays) {
            hilog.info(DOMAIN, TAG,
              'createWindowFromCpp all display id=%{public}d width=%{public}d height=%{public}d density=%{public}f',
              d.id, d.width, d.height, d.densityPixels);
          }
        })
        .catch((e: Error) => {
          hilog.error(DOMAIN, TAG, 'createWindowFromCpp getAllDisplays failed: %{public}s', String(e));
        });
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'createWindowFromCpp log display failed: %{public}s', String(e));
    }

    const context = AppWindowAdapter.getActiveContext();
    if (!context) {
      hilog.error(DOMAIN, TAG, 'createWindowFromCpp: no active UIAbilityContext');
      return;
    }

    let bundleName = '';
    try {
      const bundleInfo = bundleManager.getBundleInfoForSelfSync(
        bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION);
      bundleName = bundleInfo.name;
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'getBundleInfoForSelfSync failed: %{public}s', String(e));
      return;
    }

    const abilityName = getAbilityNameForType(options.type);
    const want: Want = {
      bundleName: bundleName,
      abilityName: abilityName,
      parameters: {
        instanceKey: String(options.windowId),
        cppWindowId: options.windowId,
        windowType: options.type,
        x: options.x ?? 0,
        y: options.y ?? 0,
        width: options.width ?? 800,
        height: options.height ?? 600,
        show: options.show ?? true,
        title: options.title ?? '',
        resizable: options.resizable ?? true,
        movable: options.movable ?? true,
        minimizable: options.minimizable ?? true,
        maximizable: options.maximizable ?? true,
        closable: options.closable ?? true,
        focusable: options.focusable ?? true,
        alwaysOnTop: options.alwaysOnTop ?? false,
        parentWindowId: options.parentWindowId ?? -1,
        parentHarmonyWindowId: options.parentHarmonyWindowId ?? -1,
        center: options.center ?? false,
        hasX: options.hasX ?? false,
        hasY: options.hasY ?? false,
        fullscreen: options.fullscreen ?? false,
        minWidth: options.minWidth ?? 0,
        minHeight: options.minHeight ?? 0,
        maxWidth: options.maxWidth ?? 0,
        maxHeight: options.maxHeight ?? 0,
        modal: options.modal ?? false,
      },
    };

    hilog.info(DOMAIN, TAG,
      'createWindowFromCpp startAbility bounds left=%{public}d top=%{public}d width=%{public}d height=%{public}d',
      bounds.left, bounds.top, bounds.width, bounds.height);

    const startOptions: StartOptions = {
      processMode: contextConstant.ProcessMode.ATTACH_TO_STATUS_BAR_ITEM,
      startupVisibility: (options.show ?? true)
        ? contextConstant.StartupVisibility.STARTUP_SHOW
        : contextConstant.StartupVisibility.STARTUP_HIDE,
      windowLeft: bounds.left,
      windowTop: bounds.top,
      windowWidth: bounds.width,
      windowHeight: bounds.height,
      displayId: resolvedDisplayId,
    };

    const param: CreateWindowParam = {
      context,
      instanceKey: String(options.windowId),
      abilityName,
      show: options.show ?? true,
      bounds,
    };

    AppWindowAdapter.startAbilityWithRetry(param, want, startOptions, 1);
  }

  // Legacy entry point used by EntryAbility to create the initial window.
  static createWindow(param: CreateWindowParam): void {
    let bundleName = '';
    try {
      const bundleInfo = bundleManager.getBundleInfoForSelfSync(
        bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION);
      bundleName = bundleInfo.name;
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'getBundleInfoForSelfSync failed: %{public}s', String(e));
      return;
    }

    const want: Want = {
      bundleName: bundleName,
      abilityName: param.abilityName,
      parameters: {
        instanceKey: param.instanceKey,
        electron_debug: AppStorage.get<string>('electron_debug') ?? '',
      },
    };

    const bounds: WindowBounds = param.bounds ?? { left: 100, top: 100, width: 800, height: 600 };
    const options: StartOptions = {
      processMode: contextConstant.ProcessMode.ATTACH_TO_STATUS_BAR_ITEM,
      startupVisibility: param.show
        ? contextConstant.StartupVisibility.STARTUP_SHOW
        : contextConstant.StartupVisibility.STARTUP_HIDE,
      windowLeft: bounds.left,
      windowTop: bounds.top,
      windowWidth: bounds.width,
      windowHeight: bounds.height,
    };

    AppWindowAdapter.startAbilityWithRetry(param, want, options, 1);
  }

  // Bind the first C++ 'main' window request to the existing EntryAbility window.
  private static bindToMainWindow(proxy: LynxWindowProxy, options: CreateWindowOptions): void {
    const mainWindow = proxy.getWindow();
    const windowStage = proxy.getWindowStage();
    const originWindowId = proxy.getOriginWindowId();
    const cppWindowId = options.windowId;

    hilog.info(DOMAIN, TAG,
      'bindToMainWindow originWindowId=%{public}d cppWindowId=%{public}d',
      originWindowId, cppWindowId);

    // Update the placeholder proxy with the real C++ id and re-register.
    AbilityManager.updateCppWindowId(proxy, cppWindowId);

    // Bind C++ window id to the HarmonyOS origin window id.
    lynxtron.setWindowIdForWindow(cppWindowId, originWindowId);

    // Apply the requested window options to the already-existing EntryAbility
    // window. The page is already loaded, so title-bar / resizable / bounds etc.
    // can be applied directly.
    AppWindowAdapter.applyWindowOptions(mainWindow, windowStage, originWindowId, options);

    // Apply bounds explicitly because EntryAbility was launched without the
    // StartOptions-driven sizing that WindowAbility uses.
    let targetX = options.x ?? 100;
    let targetY = options.y ?? 100;
    const targetWidth = options.width ?? 800;
    const targetHeight = options.height ?? 600;
    if (options.center === true && options.hasX !== true && options.hasY !== true) {
      try {
        const disp = display.getDefaultDisplaySync();
        targetX = Math.round((disp.width - targetWidth) / 2);
        targetY = Math.round((disp.height - targetHeight) / 2);
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'bindToMainWindow centerBounds failed: %{public}s', String(e));
      }
    }
    mainWindow.moveWindowToAsync(targetX, targetY)
      .then(() => mainWindow.resizeAsync(targetWidth, targetHeight))
      .then(() => hilog.info(DOMAIN, TAG, 'bindToMainWindow bounds applied %{public}d,%{public}d %{public}dx%{public}d',
        targetX, targetY, targetWidth, targetHeight))
      .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'bindToMainWindow apply bounds failed: %{public}s', JSON.stringify(e)));

    // Ensure the window is visible unless explicitly hidden.
    if (options.show !== false) {
      mainWindow.showWindow()
        .then(() => {
          hilog.info(DOMAIN, TAG, 'bindToMainWindow showWindow OK');
          reportWindowRect(originWindowId, mainWindow);
        })
        .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'bindToMainWindow showWindow failed: %{public}s', JSON.stringify(e)));
    }
  }

  // Apply window decoration / geometry options to a window. Shared between
  // BaseWindowAbility.applyWindowOptions and first-window binding.
  static applyWindowOptions(
    mainWindow: window.Window,
    windowStage: window.WindowStage,
    windowId: number,
    opts: CreateWindowOptions
  ): void {
    // Title.
    if (opts.title !== undefined && opts.title.length > 0) {
      try {
        mainWindow.setWindowTitle(opts.title);
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'setWindowTitle failed: %{public}s', String(e));
      }
    }

    // Title-bar buttons (only if the system title bar is visible).
    try {
      mainWindow.setWindowTitleButtonVisible(
        opts.maximizable ?? true,
        opts.minimizable ?? true,
        opts.closable ?? true,
      );
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'setWindowTitleButtonVisible failed: %{public}s', String(e));
    }

    // Resizable / movable.
    try {
      mainWindow.setResizeByDragEnabled(opts.resizable ?? true);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'setResizeByDragEnabled failed: %{public}s', String(e));
    }
    try {
      mainWindow.setWindowTitleMoveEnabled(opts.movable ?? true);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'setWindowTitleMoveEnabled failed: %{public}s', String(e));
    }

    // Always on top.
    if (opts.alwaysOnTop) {
      mainWindow.setWindowTopmost(true)
        .then(() => hilog.info(DOMAIN, TAG, 'setWindowTopmost OK'))
        .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'setWindowTopmost failed: %{public}s', JSON.stringify(e)));
    }

    // Bounds and initial visibility are already expressed through StartOptions
    // for Ability-launched windows. For first-window binding (EntryAbility),
    // bounds are applied separately in bindToMainWindow after UI is loaded.

    // Supported window modes reflect maximizable.
    try {
      windowStage.setSupportedWindowModes(
        (opts.maximizable ?? true)
          ? [bundleManager.SupportWindowMode.FLOATING, bundleManager.SupportWindowMode.SPLIT, bundleManager.SupportWindowMode.FULL_SCREEN]
          : [bundleManager.SupportWindowMode.FLOATING]
      );
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'setSupportedWindowModes failed: %{public}s', String(e));
    }

    // Initial fullscreen.
    if (opts.fullscreen) {
      mainWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE)
        .then(() => hilog.info(DOMAIN, TAG, 'enter-full-screen OK'))
        .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'enter-full-screen failed: %{public}s', JSON.stringify(e)));
    }

    // Size constraints. 0 means no limit on that dimension.
    if (opts.minWidth || opts.minHeight || opts.maxWidth || opts.maxHeight) {
      try {
        mainWindow.setWindowLimits({
          minWidth: opts.minWidth ?? 0,
          minHeight: opts.minHeight ?? 0,
          maxWidth: opts.maxWidth ?? 0,
          maxHeight: opts.maxHeight ?? 0,
        })
          .then(() => hilog.info(DOMAIN, TAG, 'setWindowLimits OK'))
          .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'setWindowLimits failed: %{public}s', JSON.stringify(e)));
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'setWindowLimits failed: %{public}s', String(e));
      }
    }

    // Report the initial real window rect to C++ so getWindowSize() reflects the
    // actual OS window size rather than the drawable/content size.
    reportWindowRect(windowId, mainWindow);
  }

  // Delegates to WindowOpAdapter so that window operation handling lives in one
  // place while AppWindowAdapter remains the public callback entry point.
  static registerWindowOpCallbackForWindow(proxy: LynxWindowProxy, callbacks: WindowOpCallbacks): void {
    WindowOpAdapter.registerWindowOpCallbackForWindow(proxy, callbacks);
  }

  // Entry point for C++ window operations; kept here so existing callers can
  // reach WindowOpAdapter through AppWindowAdapter.
  static handleWindowOp(windowId: number, op: string, args?: string, callbacks?: WindowOpCallbacks): void {
    WindowOpAdapter.handleWindowOp(windowId, op, args, callbacks);
  }

  // startAbility() with bounded retry on ERR_START_OPTIONS_CHECK_FAILED
  // (status-bar icon not attached yet); see the constants above.
  private static startAbilityWithRetry(
    param: CreateWindowParam,
    want: Want,
    options: StartOptions,
    attempt: number,
  ): void {
    if (attempt === 1) {
      hilog.info(DOMAIN, TAG,
        'startAbility abilityName=%{public}s instanceKey=%{public}s show=%{public}s',
        param.abilityName, param.instanceKey, String(param.show));
    }

    param.context.startAbility(want, options)
      .then(() => {
        hilog.info(DOMAIN, TAG, 'startAbility success attempt=%{public}d', attempt);
      })
      .catch((e: BusinessError) => {
        if (e?.code === ERR_START_OPTIONS_CHECK_FAILED && attempt < START_MAX_ATTEMPTS) {
          hilog.warn(DOMAIN, TAG,
            'startAbility 16000067, retry %{public}d/%{public}d in %{public}dms',
            attempt, START_MAX_ATTEMPTS, START_RETRY_INTERVAL_MS);
          setTimeout(() => {
            AppWindowAdapter.startAbilityWithRetry(param, want, options, attempt + 1);
          }, START_RETRY_INTERVAL_MS);
          return;
        }
        hilog.error(DOMAIN, TAG,
          'startAbility failed after %{public}d attempt(s): %{public}d %{public}s',
          attempt, e?.code ?? -1, e?.message ?? String(e));
      });
  }

  private static getActiveContext(): common.UIAbilityContext | undefined {
    // The active context is the context of the currently running Ability.
    // For the first window it is EntryAbility; for subsequent windows it can
    // be any existing WindowAbility. We retrieve it from the global app storage
    // set by BaseWindowAbility.onWindowStageCreate.
    const stored = AppStorage.get<common.UIAbilityContext>('active_ability_context');
    if (stored && AbilityManager.getByContext(stored)) {
      return stored;
    }

    // The stored context may belong to an Ability that has already been
    // destroyed (e.g. its window was closed). Fall back to any living Ability
    // so that subsequent startAbility() calls do not fail with a stale context.
    const windowIds = AbilityManager.getAllWindowIds();
    for (const id of windowIds) {
      const proxy = AbilityManager.get(id);
      if (proxy) {
        AppStorage.setOrCreate('active_ability_context', proxy.getWindowContext());
        return proxy.getWindowContext();
      }
    }

    // Final fallback to EntryAbility's context if no WindowAbility is alive.
    const entryContext = AppStorage.get<common.UIAbilityContext>('abilityContext');
    if (entryContext) {
      AppStorage.setOrCreate('active_ability_context', entryContext);
    }
    return entryContext;
  }
}
