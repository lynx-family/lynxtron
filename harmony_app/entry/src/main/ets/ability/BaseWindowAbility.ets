import UIAbility from '@ohos.app.ability.UIAbility';
import common from '@ohos.app.ability.common';
import Want from '@ohos.app.ability.Want';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import hilog from '@ohos.hilog';
import window from '@ohos.window';
import display from '@ohos.display';
import backgroundTaskManager from '@ohos.backgroundTaskManager';
import bundleManager from '@ohos.bundle.bundleManager';
import { BusinessError } from '@ohos.base';
import lynxtron from 'liblynxtron_napi.so';
import { AbilityManager } from '../common/AbilityManager';
import { LynxWindowProxy } from '../common/LynxWindowProxy';
import { CreateWindowOptions, WindowType } from '../common/WindowTypes';
import { AppWindowAdapter } from '../adapter/AppWindowAdapter';
import { reportWindowRect } from '../utils/WindowUtils';

// Matches gfx::ResizeEdge in shell/ui/gfx/geometry/resize_utils.h and the
// plain-C enum duplicated in lynxtron_napi_bridge.cc / native_window_harmony.cc.
const RESIZE_EDGE_BOTTOM = 0;
const RESIZE_EDGE_BOTTOM_LEFT = 1;
const RESIZE_EDGE_BOTTOM_RIGHT = 2;
const RESIZE_EDGE_LEFT = 3;
const RESIZE_EDGE_RIGHT = 4;
const RESIZE_EDGE_TOP = 5;
const RESIZE_EDGE_TOP_LEFT = 6;
const RESIZE_EDGE_TOP_RIGHT = 7;

const DOMAIN = 0x0000;
const TAG = 'BaseWindowAbility';

export default abstract class BaseWindowAbility extends UIAbility implements LynxWindowProxy {
  protected windowId: number = -1;
  protected originWindowId: number = -1;
  // Launch parameters parsed from the Want; defaults to a placeholder main
  // window before the real C++ window id is bound.
  protected launchOptions: CreateWindowOptions = { windowId: 0, type: 'main' };
  protected suspendDelayInfo?: backgroundTaskManager.DelaySuspendInfo;
  private mainWindow?: window.Window;
  private windowStage?: window.WindowStage;

  static registerWindowLifecycleListeners(windowId: number, win: window.Window): void {
    if (windowId < 0) {
      hilog.warn(DOMAIN, TAG, 'registerWindowLifecycleListeners: invalid windowId');
      return;
    }

    // Keep C++ window-state cache and JS events in sync with manual
    // minimize/maximize/fullscreen/restore.
    try {
      let lastStatus = window.WindowStatusType.UNDEFINED;
      win.on('windowStatusChange', (status: window.WindowStatusType) => {
        if (status === lastStatus) {
          return;
        }
        const previousStatus = lastStatus;
        lastStatus = status;

        hilog.info(DOMAIN, TAG,
          'windowStatusChange windowId=%{public}d prev=%{public}d curr=%{public}d',
          windowId, previousStatus, status);

        // Transitions out of fullscreen.
        if (previousStatus === window.WindowStatusType.FULL_SCREEN) {
          lynxtron.notifyWindowState(windowId, 'leave-full-screen');
        }

        switch (status) {
          case window.WindowStatusType.MINIMIZE:
            lynxtron.notifyWindowState(windowId, 'minimize');
            break;
          case window.WindowStatusType.MAXIMIZE:
            lynxtron.notifyWindowState(windowId, 'maximize');
            break;
          case window.WindowStatusType.FULL_SCREEN:
            lynxtron.notifyWindowState(windowId, 'enter-full-screen');
            break;
          default:
          // FLOATING, SPLIT_SCREEN, UNDEFINED: window is back to a normal
          // (non-minimized, non-maximized) state.
            if (previousStatus === window.WindowStatusType.MINIMIZE ||
              previousStatus === window.WindowStatusType.MAXIMIZE) {
              lynxtron.notifyWindowState(windowId, 'restore');
            }
            break;
        }
      });
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'register windowStatusChange failed: %{public}s', String(e));
    }

    // Detect manual show/hide/close for both main Ability windows and subwindows.
    try {
      win.on('windowEvent', (event: window.WindowEventType) => {
        if (event === window.WindowEventType.WINDOW_DESTROYED) {
          hilog.info(DOMAIN, TAG, 'windowEvent WINDOW_DESTROYED windowId=%{public}d', windowId);
          lynxtron.notifyWindowState(windowId, 'closed');
        } else if (event === window.WindowEventType.WINDOW_SHOWN) {
          hilog.info(DOMAIN, TAG, 'windowEvent WINDOW_SHOWN windowId=%{public}d', windowId);
          lynxtron.notifyWindowState(windowId, 'show');
        } else if (event === window.WindowEventType.WINDOW_HIDDEN) {
          hilog.info(DOMAIN, TAG, 'windowEvent WINDOW_HIDDEN windowId=%{public}d', windowId);
          lynxtron.notifyWindowState(windowId, 'hide');
        } else if (event === window.WindowEventType.WINDOW_INACTIVE) {
          hilog.info(DOMAIN, TAG, 'windowEvent WINDOW_INACTIVE windowId=%{public}d', windowId);
          lynxtron.notifyWindowState(windowId, 'blur');
        } else if (event === window.WindowEventType.WINDOW_ACTIVE) {
          hilog.info(DOMAIN, TAG, 'windowEvent WINDOW_ACTIVE windowId=%{public}d', windowId);
          lynxtron.notifyWindowState(windowId, 'focus');
        }
      });
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'register windowEvent failed: %{public}s', String(e));
    }

    // Forward resize completion to C++ so JS 'resized' fires. DRAG_END means
    // the user has finished a drag-resize; RECOVER/MAXIMIZE cover restore and
    // maximize transitions that also change the window size.
    //
    // The resize edge must not be derived from getLastPointerPosition(): while
    // dragging a window edge the pointer lives in the window frame (outside the
    // XComponent), so the cached position is stale and wildly wrong for the
    // top/bottom edges. Instead, snapshot the rect at DRAG_START and diff it
    // against the first DRAG rect — the edge that moved is the one being
    // dragged. 'will-resize' therefore fires on the first DRAG instead of
    // DRAG_START, but with a correct edge.
    try {
      let dragStartRect: window.Rect | null = null;
      win.on('windowRectChange', (option: window.RectChangeOptions) => {
        const reason = option.reason;

        // Report the actual OS window rect (windowRect) back to C++. This is
        // independent of the existing drag/resize event handling and does not
        // modify the original logic below.
        if (option.rect) {
          reportWindowRect(windowId, win);
        }

        if (reason === window.RectChangeReason.DRAG_START && option.rect) {
          dragStartRect = option.rect;
          return;
        }

        if (reason === window.RectChangeReason.DRAG && option.rect && dragStartRect) {
          const edge = BaseWindowAbility.computeResizeEdgeFromDelta(dragStartRect, option.rect);
          dragStartRect = null;
          hilog.info(DOMAIN, TAG,
            'windowRectChange windowId=%{public}d reason=%{public}d will-resize edge=%{public}d',
            windowId, reason, edge);
          lynxtron.notifyWindowState(windowId, 'will-resize', edge);
          return;
        }

        if (reason === window.RectChangeReason.DRAG_END) {
          dragStartRect = null;
          hilog.info(DOMAIN, TAG,
            'windowRectChange windowId=%{public}d reason=%{public}d',
            windowId, reason);
          lynxtron.notifyWindowState(windowId, 'resized');
          return;
        }
      });
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'register windowRectChange failed: %{public}s', String(e));
    }
  }

  // Determines which edge/corner was dragged by diffing the rect captured at
  // DRAG_START against the first DRAG rect. The edge whose coordinate moved is
  // the one being resized. Returns a gfx::ResizeEdge value so it can be passed
  // straight to notifyWindowState(..., 'will-resize', edge).
  private static computeResizeEdgeFromDelta(before: window.Rect, after: window.Rect): number {
    const leftChanged = before.left != after.left;
    const rightChanged = (before.left + before.width) != (after.left + after.width);
    const topChanged = before.top != after.top;
    const bottomChanged = (before.top + before.height) != (after.top + after.height);

    if (leftChanged && topChanged) {
      return RESIZE_EDGE_TOP_LEFT;
    }
    if (rightChanged && topChanged) {
      return RESIZE_EDGE_TOP_RIGHT;
    }
    if (leftChanged && bottomChanged) {
      return RESIZE_EDGE_BOTTOM_LEFT;
    }
    if (rightChanged && bottomChanged) {
      return RESIZE_EDGE_BOTTOM_RIGHT;
    }
    if (leftChanged) {
      return RESIZE_EDGE_LEFT;
    }
    if (rightChanged) {
      return RESIZE_EDGE_RIGHT;
    }
    if (topChanged) {
      return RESIZE_EDGE_TOP;
    }
    return RESIZE_EDGE_BOTTOM;
  }

  // ---- LynxWindowProxy implementation ----

  getWindow(): window.Window {
    if (!this.mainWindow) {
      throw new Error('BaseWindowAbility.getWindow: mainWindow not available');
    }
    return this.mainWindow;
  }

  getWindowStage(): window.WindowStage {
    if (!this.windowStage) {
      throw new Error('BaseWindowAbility.getWindowStage: windowStage not available');
    }
    return this.windowStage;
  }

  getWindowContext(): common.UIAbilityContext {
    return this.context;
  }

  getWindowId(): number {
    return this.windowId;
  }

  getOriginWindowId(): number {
    return this.originWindowId;
  }

  getCppWindowId(): number {
    return this.launchOptions.windowId;
  }

  setCppWindowId(id: number): void {
    this.launchOptions.windowId = id;
  }

  isMainAbility(): boolean {
    return this.launchOptions.type === 'main';
  }

  // Parses the launch parameters carried in the Want into launchOptions.
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    const params = want.parameters as Record<string, Object> | undefined;
    this.launchOptions = {
      // The C++ driven path (AppWindowAdapter.createWindowFromCpp) puts the
      // C++-allocated window id under the key "cppWindowId"; the legacy
      // EntryAbility path had no id. Use 0 as the placeholder for the first
      // (EntryAbility) window before it is bound to a C++ window id.
      windowId: Number(params?.['cppWindowId'] ?? params?.['windowId'] ?? 0),
      type: String(params?.['windowType'] ?? params?.['type'] ?? 'main') as WindowType,
      x: Number(params?.['x'] ?? 0),
      y: Number(params?.['y'] ?? 0),
      width: Number(params?.['width'] ?? 800),
      height: Number(params?.['height'] ?? 600),
      show: params?.['show'] === undefined ? true : Boolean(params?.['show']),
      title: params?.['title'] !== undefined ? String(params?.['title']) : undefined,
      resizable: params?.['resizable'] === undefined ? true : Boolean(params?.['resizable']),
      movable: params?.['movable'] === undefined ? true : Boolean(params?.['movable']),
      minimizable: params?.['minimizable'] === undefined ? true : Boolean(params?.['minimizable']),
      maximizable: params?.['maximizable'] === undefined ? true : Boolean(params?.['maximizable']),
      closable: params?.['closable'] === undefined ? true : Boolean(params?.['closable']),
      focusable: params?.['focusable'] === undefined ? true : Boolean(params?.['focusable']),
      alwaysOnTop: params?.['alwaysOnTop'] === undefined ? false : Boolean(params?.['alwaysOnTop']),
      parentWindowId: params?.['parentWindowId'] !== undefined ? Number(params?.['parentWindowId']) : undefined,
      parentHarmonyWindowId: params?.['parentHarmonyWindowId'] !== undefined ? Number(params?.['parentHarmonyWindowId']) : undefined,
      fullscreen: params?.['fullscreen'] === undefined ? false : Boolean(params?.['fullscreen']),
      minWidth: params?.['minWidth'] !== undefined ? Number(params?.['minWidth']) : undefined,
      minHeight: params?.['minHeight'] !== undefined ? Number(params?.['minHeight']) : undefined,
      maxWidth: params?.['maxWidth'] !== undefined ? Number(params?.['maxWidth']) : undefined,
      maxHeight: params?.['maxHeight'] !== undefined ? Number(params?.['maxHeight']) : undefined,
      modal: params?.['modal'] === undefined ? false : Boolean(params?.['modal']),
    };

    hilog.info(DOMAIN, TAG,
      'onCreate instanceKey=%{public}s launchReason=%{public}d cppWindowId=%{public}d type=%{public}s',
      String(want.parameters?.['instanceKey']), launchParam.launchReason,
      this.launchOptions.windowId, this.launchOptions.type);
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG,
      'onNewWant instanceKey=%{public}s launchReason=%{public}d',
      String(want.parameters?.['instanceKey']), launchParam.launchReason);
  }

  onDestroy(): void {
    hilog.info(DOMAIN, TAG, 'onDestroy windowId=%{public}d', this.windowId);
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, 'onWindowStageCreate');
    try {
      const mainWindow = windowStage.getMainWindowSync();
      const originWindowId = mainWindow.getWindowProperties().id;
      this.originWindowId = originWindowId;
      this.windowId = originWindowId;
      this.mainWindow = mainWindow;
      this.windowStage = windowStage;

      // Diagnostic logging for multi-display / mirror-mode issues.
      try {
        const props = mainWindow.getWindowProperties();
        hilog.info(DOMAIN, TAG,
          'onWindowStageCreate mainWindow displayId=%{public}d windowId=%{public}d',
          props?.displayId, this.windowId);
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'onWindowStageCreate get displayId failed: %{public}s', String(e));
      }

      // Publish this Ability's context so that AppWindowAdapter can use it to
      // start subsequent windows.
      AppStorage.setOrCreate('active_ability_context', this.context);

      // Register this Ability as a LynxWindowProxy. For the first (EntryAbility)
      // window the C++ id is 0 and acts as a placeholder until the first
      // createWindowFromCpp('main') request binds a real C++ id.
      AbilityManager.addProxy(this);
      const cppWindowId = this.launchOptions.windowId;
      if (cppWindowId > 0) {
        lynxtron.setWindowIdForWindow(cppWindowId, originWindowId);
      } else {
        hilog.info(DOMAIN, TAG,
          'placeholder proxy registered originWindowId=%{public}d, awaiting first main window binding',
          originWindowId);
      }

      this.registerWindowOpCallback();

      // Load the page first; window decoration options (title, buttons,
      // resizable/movable, topmost, supported modes) are applied in the
      // loadContent callback once the window is actually ready.
      this.onWindowCreated(windowStage, mainWindow);

      // register window events
      BaseWindowAbility.registerWindowLifecycleListeners(this.windowId, mainWindow);

      hilog.info(DOMAIN, TAG,
        'windowId=%{public}d originWindowId=%{public}d cppWindowId=%{public}d registered',
        this.windowId, originWindowId, cppWindowId);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'window setup failed: %{public}s', String(e));
    }
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, TAG, 'onWindowStageDestroy windowId=%{public}d', this.windowId);
    AbilityManager.removeProxy(this);

    // If the Ability being destroyed is the current active context, the global
    // active context must be migrated to another living Ability. Otherwise
    // subsequent createWindowFromCpp calls will try to start an Ability from a
    // stale, terminated context and fail silently.
    const stored = AppStorage.get<common.UIAbilityContext>('active_ability_context');
    if (stored === this.context) {
      let fallback: common.UIAbilityContext | undefined;
      for (const id of AbilityManager.getAllWindowIds()) {
        const proxy = AbilityManager.get(id);
        if (proxy?.getWindowContext() && proxy.getWindowContext() !== this.context) {
          fallback = proxy.getWindowContext();
          break;
        }
      }
      if (!fallback) {
        fallback = AppStorage.get<common.UIAbilityContext>('abilityContext');
      }
      if (fallback) {
        AppStorage.setOrCreate('active_ability_context', fallback);
      }
    }
  }

  onForeground(): void {
    hilog.info(DOMAIN, TAG, 'onForeground windowId=%{public}d', this.windowId);
  }

  onBackground(): void {
    hilog.info(DOMAIN, TAG, 'onBackground windowId=%{public}d', this.windowId);
    this.requestSuspendDelayForWindowOps();
  }

  protected abstract onWindowCreated(windowStage: window.WindowStage, mainWindow: window.Window): void;

  // Subclasses call this from their loadContent success callback. Several
  // window decoration APIs (title, title-bar buttons, drag/move, topmost,
  // supported modes) must not be used before the window/page is ready.
  protected applyWindowOptions(mainWindow: window.Window, windowStage: window.WindowStage): void {
    AppWindowAdapter.applyWindowOptions(mainWindow, windowStage, this.windowId, this.launchOptions);
  }

  // Keep the Ability alive long enough to perform window operations (especially
  // restore) after the window has been minimized and the Ability has moved to
  // the background. Without this, ArkTS window APIs such as restore() return
  // 1300004 because the Ability event loop is suspended.
  private requestSuspendDelayForWindowOps(): void {
    try {
      if (this.suspendDelayInfo) {
        backgroundTaskManager.cancelSuspendDelay(this.suspendDelayInfo.requestId);
      }
      this.suspendDelayInfo = backgroundTaskManager.requestSuspendDelay('window-ops', () => {
        hilog.info(DOMAIN, TAG, 'suspend delay for window ops is about to expire');
      });
      hilog.info(DOMAIN, TAG, 'requestSuspendDelay success id=%{public}d', this.suspendDelayInfo.requestId);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'requestSuspendDelay failed: %{public}s', String(e));
    }
  }

  // Registers the C++ window-op callback for this Ability's window, wiring
  // minimize suspend-delay and showAbility into WindowOpAdapter.
  private registerWindowOpCallback(): void {
    AppWindowAdapter.registerWindowOpCallbackForWindow(this, {
      onMinimize: () => this.requestSuspendDelayForWindowOps(),
      showAbility: () => this.context.showAbility(),
    });
  }
}
