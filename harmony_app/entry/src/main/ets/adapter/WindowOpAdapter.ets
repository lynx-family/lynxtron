import common from '@ohos.app.ability.common';
import hilog from '@ohos.hilog';
import window from '@ohos.window';
import { BusinessError } from '@ohos.base';
import lynxtron from 'liblynxtron_napi.so';
import { AbilityManager } from '../common/AbilityManager';
import { LynxWindowProxy } from '../common/LynxWindowProxy';
import { WindowBounds } from '../common/WindowTypes';

const DOMAIN = 0x0000;
const TAG = 'WindowOpAdapter';

// Callbacks supplied by the Ability that owns the window. Subwindows do not
// provide these because they use window.Window verbs directly.
export interface WindowOpCallbacks {
  onMinimize?: () => void;
  showAbility?: () => Promise<void>;
}

// JSON payloads received from the C++ window operation bridge.
type ParentWindowIdPayload = number;

// min/max size constraints for the setWindowLimits op (0 = no limit).
interface WindowLimitsPayload {
  minWidth: number;
  minHeight: number;
  maxWidth: number;
  maxHeight: number;
}

// Frame border widths between the OS window rect and the drawable rect.
interface WindowBorders {
  leftBorder: number;
  topBorder: number;
  rightBorder: number;
  bottomBorder: number;
}

// Window origin for the setPosition op.
interface WindowPosition {
  x: number;
  y: number;
}

// Window size for the setSize op.
interface WindowSize {
  width: number;
  height: number;
}

export class WindowOpAdapter {
  // Register the C++ window operation callback for a proxy. The callback
  // forwards operations to WindowOpAdapter.handleWindowOp.
  static registerWindowOpCallbackForWindow(proxy: LynxWindowProxy, callbacks: WindowOpCallbacks): void {
    lynxtron.registerWindowOpCallbackForWindow(proxy.getOriginWindowId(), (_id: number, op: string, args?: string) => {
      WindowOpAdapter.handleWindowOp(proxy.getOriginWindowId(), op, args, callbacks);
    });
  }

  // Central dispatcher for C++ window operation requests.
  static handleWindowOp(windowId: number, op: string, args?: string, callbacks?: WindowOpCallbacks): void {
    hilog.info(DOMAIN, TAG, 'windowOp %{public}s args=%{public}s', op, args ?? '');
    const proxy = AbilityManager.get(windowId);
    if (!proxy) {
      hilog.warn(DOMAIN, TAG, 'windowOp %{public}s no proxy for windowId=%{public}d', op, windowId);
      return;
    }

    try {
      const win = proxy.getWindow();
      const windowStatus = win.getWindowStatus();
      const isMinimized = windowStatus === window.WindowStatusType.MINIMIZE;
      hilog.info(DOMAIN, TAG,
        'windowOp %{public}s status=%{public}d minimized=%{public}s',
        op, windowStatus, String(isMinimized));

      switch (op) {
        case 'minimize':
          if (callbacks?.onMinimize) callbacks.onMinimize();
          win.minimize()
            .then(() => WindowOpAdapter.recordWindowOpResult('minimize', 'OK'))
            .catch((e: Error) => WindowOpAdapter.recordWindowOpResult('minimize', 'ERR', String(e)));
          break;
        case 'maximize':
          win.maximize(window.MaximizePresentation.EXIT_IMMERSIVE)
            .then(() => WindowOpAdapter.recordWindowOpResult('maximize', 'OK'))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('maximize', 'ERR', WindowOpAdapter.errText(e)));
          break;
        case 'enter-full-screen':
          win.maximize(window.MaximizePresentation.ENTER_IMMERSIVE)
            .then(() => WindowOpAdapter.recordWindowOpResult('enter-full-screen', 'OK'))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('enter-full-screen', 'ERR', WindowOpAdapter.errText(e)));
          break;
        case 'leave-full-screen':
          win.recover()
            .then(() => WindowOpAdapter.recordWindowOpResult('leave-full-screen', 'OK'))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('leave-full-screen', 'ERR', WindowOpAdapter.errText(e)));
          break;
        case 'restore':
          if (windowStatus === window.WindowStatusType.FULL_SCREEN ||
              windowStatus === window.WindowStatusType.MAXIMIZE) {
            win.recover()
              .then(() => WindowOpAdapter.recordWindowOpResult('restore', 'OK', 'recover'))
              .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('restore', 'ERR', `recover:${WindowOpAdapter.errText(e)}`));
          } else if (windowStatus === window.WindowStatusType.MINIMIZE) {
            if (callbacks?.onMinimize) callbacks.onMinimize();
            win.restore()
              .then(() => WindowOpAdapter.recordWindowOpResult('restore', 'OK', 'restore'))
              .catch((e: BusinessError) => {
                WindowOpAdapter.recordWindowOpResult('restore', 'ERR', `restore:${WindowOpAdapter.errText(e)}`);
                if (callbacks?.showAbility) {
                  callbacks.showAbility()
                    .then(() => WindowOpAdapter.recordWindowOpResult('restore', 'OK', 'showAbility-fallback'))
                    .catch((e2: BusinessError) => WindowOpAdapter.recordWindowOpResult('restore', 'ERR', `showAbility:${WindowOpAdapter.errText(e2)}`));
                }
              });
          } else {
            WindowOpAdapter.recordWindowOpResult('restore', 'OK', `already-normal status=${windowStatus}`);
          }
          break;
        case 'show':
          // Main Ability windows that were hidden via hideAbility() must be
          // re-shown via showAbility(); subwindows use showWindow().
          if (callbacks?.showAbility) {
            callbacks.showAbility()
              .then(() => WindowOpAdapter.recordWindowOpResult('show', 'OK', 'showAbility'))
              .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('show', 'ERR', `showAbility:${WindowOpAdapter.errText(e)}`));
          } else {
            win.showWindow()
              .then(() => WindowOpAdapter.recordWindowOpResult('show', 'OK', 'showWindow'))
              .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('show', 'ERR', `showWindow:${WindowOpAdapter.errText(e)}`));
          }
          break;
        case 'close':
          // For subwindows win.destroyWindow() closes just the subwindow;
          // for main Ability windows terminateSelf() ends the Ability.
          if (callbacks?.showAbility) {
            proxy.getWindowContext().terminateSelf()
              .then(() => WindowOpAdapter.recordWindowOpResult('close', 'OK', 'terminateSelf'))
              .catch((e: Error) => WindowOpAdapter.recordWindowOpResult('close', 'ERR', `terminateSelf:${String(e)}`));
          } else {
            win.destroyWindow()
              .then(() => WindowOpAdapter.recordWindowOpResult('close', 'OK', 'destroyWindow'))
              .catch((e: Error) => WindowOpAdapter.recordWindowOpResult('close', 'ERR', `destroyWindow:${String(e)}`));
          }
          break;
        case 'focus': {
          const focusValue = args ? JSON.parse(args) as boolean : true;
          if (focusValue === false) {
            win.setWindowFocusable(false)
              .then(() => WindowOpAdapter.recordWindowOpResult('focus', 'OK', 'setWindowFocusable-false'))
              .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('focus', 'ERR', `setWindowFocusable-false:${WindowOpAdapter.errText(e)}`));
          } else if (callbacks?.showAbility) {
            callbacks.showAbility()
              .then(() => WindowOpAdapter.recordWindowOpResult('focus', 'OK', 'showAbility'))
              .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('focus', 'ERR', `showAbility:${WindowOpAdapter.errText(e)}`));
          } else {
            win.setWindowFocusable(true)
              .then(() => win.showWindow())
              .then(() => WindowOpAdapter.recordWindowOpResult('focus', 'OK', 'setWindowFocusable-true-showWindow'))
              .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('focus', 'ERR', `focus-true:${WindowOpAdapter.errText(e)}`));
          }
          break;
        }
        case 'setAlwaysOnTop': {
          const onTopValue = args ? JSON.parse(args) as boolean : false;
          win.setWindowTopmost(onTopValue)
            .then(() => WindowOpAdapter.recordWindowOpResult('setAlwaysOnTop', 'OK'))
            .catch((e: Error) => WindowOpAdapter.recordWindowOpResult('setAlwaysOnTop', 'ERR', String(e)));
          break;
        }
        case 'setTitle': {
          const title = args ? JSON.parse(args) as string : '';
          win.setWindowTitle(title)
            .then(() => WindowOpAdapter.recordWindowOpResult('setTitle', 'OK'))
            .catch((e: Error) => WindowOpAdapter.recordWindowOpResult('setTitle', 'ERR', String(e)));
          break;
        }
        case 'setBounds': {
          const bounds = args ? JSON.parse(args) as WindowBounds : undefined;
          if (!bounds) {
            WindowOpAdapter.recordWindowOpResult('setBounds', 'ERR', 'missing-args');
            break;
          }
          win.moveWindowToAsync(bounds.left, bounds.top)
            .then(() => win.resizeAsync(bounds.width, bounds.height))
            .then(() => WindowOpAdapter.recordWindowOpResult('setBounds', 'OK'))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('setBounds', 'ERR', WindowOpAdapter.errText(e)));
          break;
        }
        case 'setPosition': {
          const pos = args ? JSON.parse(args) as WindowPosition : undefined;
          if (!pos) {
            WindowOpAdapter.recordWindowOpResult('setPosition', 'ERR', 'missing-args');
            break;
          }
          win.moveWindowToAsync(pos.x, pos.y)
            .then(() => WindowOpAdapter.recordWindowOpResult('setPosition', 'OK'))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('setPosition', 'ERR', WindowOpAdapter.errText(e)));
          break;
        }
        case 'setSize': {
          const size = args ? JSON.parse(args) as WindowSize : undefined;
          if (!size) {
            WindowOpAdapter.recordWindowOpResult('setSize', 'ERR', 'missing-args');
            break;
          }
          win.resizeAsync(size.width, size.height)
            .then(() => WindowOpAdapter.recordWindowOpResult('setSize', 'OK'))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('setSize', 'ERR', WindowOpAdapter.errText(e)));
          break;
        }
        case 'hide':
          // Main Ability windows are hidden via the Ability context; subwindows
          // do not have a separate hide API, so minimize them instead.
          if (callbacks?.showAbility) {
            proxy.getWindowContext().hideAbility()
              .then(() => WindowOpAdapter.recordWindowOpResult('hide', 'OK', 'hideAbility'))
              .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('hide', 'ERR', `hideAbility:${WindowOpAdapter.errText(e)}`));
          } else {
            win.minimize()
              .then(() => WindowOpAdapter.recordWindowOpResult('hide', 'OK', 'minimize-fallback'))
              .catch((e: Error) => WindowOpAdapter.recordWindowOpResult('hide', 'ERR', String(e)));
          }
          break;
        case 'setParentWindow': {
          const parentWindowId = args ? JSON.parse(args) as ParentWindowIdPayload : -1;
          if (parentWindowId <= 0) {
            WindowOpAdapter.recordWindowOpResult('setParentWindow', 'ERR', 'invalid-parent-window-id');
            break;
          }
          win.setParentWindow(parentWindowId)
            .then(() => WindowOpAdapter.recordWindowOpResult(
              'setParentWindow', 'OK', `parentWindowId=${parentWindowId}`))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult(
              'setParentWindow', 'ERR', WindowOpAdapter.errText(e)));
          break;
        }
        case 'setWindowLimits': {
          const limits = args ? JSON.parse(args) as WindowLimitsPayload : undefined;
          if (!limits) {
            WindowOpAdapter.recordWindowOpResult('setWindowLimits', 'ERR', 'missing-args');
            break;
          }
          const borders = WindowOpAdapter.getWindowBorders(win);
          win.setWindowLimits({
            minWidth: limits.minWidth === 0 ? 0 : limits.minWidth + borders.leftBorder + borders.rightBorder,
            minHeight: limits.minHeight === 0 ? 0 : limits.minHeight + borders.topBorder + borders.bottomBorder,
            maxWidth: limits.maxWidth === 0 ? 0 : limits.maxWidth + borders.leftBorder + borders.rightBorder,
            maxHeight: limits.maxHeight === 0 ? 0 : limits.maxHeight + borders.topBorder + borders.bottomBorder,
          })
            .then(() => WindowOpAdapter.recordWindowOpResult('setWindowLimits', 'OK'))
            .catch((e: BusinessError) => WindowOpAdapter.recordWindowOpResult('setWindowLimits', 'ERR', WindowOpAdapter.errText(e)));
          break;
        }
        default:
          hilog.warn(DOMAIN, TAG, 'unknown windowOp %{public}s', op);
      }
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'windowOp %{public}s failed: %{public}s', op, String(e));
    }
  }

  // Formats a BusinessError as "<code> <message>".
  private static errText(e: BusinessError): string {
    return e ? `${e.code} ${e.message}` : 'unknown error';
  }

  // Logs the outcome of a window operation for logcat diagnosis.
  private static recordWindowOpResult(op: string, result: string, detail?: string): void {
    hilog.info(DOMAIN, TAG, 'windowOp %{public}s %{public}s %{public}s', op, result, detail ?? '');
  }

  // Computes the frame border widths between windowRect and drawableRect so
  // size limits can be converted from drawable to OS window coordinates.
  private static getWindowBorders(win: window.Window): WindowBorders {
    const props = win.getWindowProperties();
    const drawable = props?.drawableRect;
    const windowRect = props?.windowRect;
    const left = drawable?.left ?? 0;
    const top = drawable?.top ?? 0;
    const drawableWidth = drawable?.width ?? 0;
    const drawableHeight = drawable?.height ?? 0;
    const windowWidth = windowRect?.width ?? (left + drawableWidth);
    const windowHeight = windowRect?.height ?? (top + drawableHeight);
    return {
      leftBorder: left,
      topBorder: top,
      rightBorder: Math.max(0, windowWidth - drawableWidth - left),
      bottomBorder: Math.max(0, windowHeight - drawableHeight - top),
    };
  }
}
