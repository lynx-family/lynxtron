import common from '@ohos.app.ability.common';
import hilog from '@ohos.hilog';
import window from '@ohos.window';
import { BusinessError } from '@ohos.base';
import lynxtron from 'liblynxtron_napi.so';
import { AbilityManager } from '../common/AbilityManager';
import { LynxWindowProxy } from '../common/LynxWindowProxy';
import { CreateWindowOptions, WindowBounds } from '../common/WindowTypes';
import { WindowOpAdapter } from './WindowOpAdapter';

const DOMAIN = 0x0000;
const TAG = 'SubWindowAdapter';

// Proxy wrapper for OS subwindows created via windowStage.createSubWindowWithOptions().
// Subwindows share their parent's Ability context and window stage but have their
// own window.Window object and HarmonyOS window id.
class SubWindowProxy implements LynxWindowProxy {
  private readonly subWindow: window.Window;
  private readonly parentContext: common.UIAbilityContext;
  private readonly parentStage: window.WindowStage;
  private readonly originWindowId: number;
  private cppWindowId: number;

  constructor(
    subWindow: window.Window,
    parentContext: common.UIAbilityContext,
    parentStage: window.WindowStage,
    originWindowId: number,
    cppWindowId: number
  ) {
    this.subWindow = subWindow;
    this.parentContext = parentContext;
    this.parentStage = parentStage;
    this.originWindowId = originWindowId;
    this.cppWindowId = cppWindowId;
  }

  getWindow(): window.Window {
    return this.subWindow;
  }

  getWindowStage(): window.WindowStage {
    return this.parentStage;
  }

  getWindowContext(): common.UIAbilityContext {
    return this.parentContext;
  }

  getWindowId(): number {
    return this.originWindowId;
  }

  getOriginWindowId(): number {
    return this.originWindowId;
  }

  getCppWindowId(): number {
    return this.cppWindowId;
  }

  setCppWindowId(id: number): void {
    this.cppWindowId = id;
  }

  isMainAbility(): boolean {
    return false;
  }
}

// Centers the given bounds within the parent window's current rect.
function centerBounds(bounds: WindowBounds, parentWindow: window.Window): WindowBounds {
  try {
    const parentRect = parentWindow.getWindowProperties().windowRect;
    const left = parentRect?.left ?? 0;
    const top = parentRect?.top ?? 0;
    const width = parentRect?.width ?? 1920;
    const height = parentRect?.height ?? 1080;
    return {
      left: Math.round(left + (width - bounds.width) / 2),
      top: Math.round(top + (height - bounds.height) / 2),
      width: bounds.width,
      height: bounds.height,
    };
  } catch (e) {
    hilog.warn(DOMAIN, TAG, 'centerBounds failed: %{public}s', String(e));
    return bounds;
  }
}

export class SubWindowAdapter {
  // Creates a decorated OS subwindow under the parent's window stage, applies
  // its options (title, bounds, limits, ...), registers the proxy and loads
  // pages/Index into it.
  static createSubWindow(parentProxy: LynxWindowProxy, options: CreateWindowOptions): void {
    const parentStage = parentProxy.getWindowStage();
    const parentContext = parentProxy.getWindowContext();
    const parentWindow = parentProxy.getWindow();
    const name = `lynxtron-sub-${options.windowId}`;
    hilog.info(DOMAIN, TAG, 'createSubWindow name=%{public}s', name);

    const subWindowOptions: window.SubWindowOptions = {
      title: options.title ?? '',
      decorEnabled: true,
      isModal: options.modal ?? false,
      maximizeSupported: options.maximizable ?? true,
      outlineEnabled: false,
    };
    parentStage.createSubWindowWithOptions(name, subWindowOptions)
      .then((subWindow) => {
        const originWindowId = subWindow.getWindowProperties().id;
        const proxy = new SubWindowProxy(subWindow, parentContext, parentStage, originWindowId, options.windowId);
        AbilityManager.addProxy(proxy);

        // Title and title-bar buttons.
        try {
          subWindow.setWindowTitle(options.title ?? '');
        } catch (e) {
          hilog.error(DOMAIN, TAG, 'subWindow setWindowTitle failed: %{public}s', String(e));
        }
        try {
          subWindow.setWindowTitleButtonVisible(
            options.maximizable ?? true,
            options.minimizable ?? true,
            options.closable ?? true
          );
        } catch (e) {
          hilog.error(DOMAIN, TAG, 'subWindow setWindowTitleButtonVisible failed: %{public}s', String(e));
        }
        try {
          subWindow.setResizeByDragEnabled(options.resizable ?? true);
        } catch (e) {
          hilog.error(DOMAIN, TAG, 'subWindow setResizeByDragEnabled failed: %{public}s', String(e));
        }
        try {
          subWindow.setWindowTitleMoveEnabled(options.movable ?? true);
        } catch (e) {
          hilog.error(DOMAIN, TAG, 'subWindow setWindowTitleMoveEnabled failed: %{public}s', String(e));
        }

        // Bounds.
        let targetX = options.x ?? 0;
        let targetY = options.y ?? 0;
        const targetWidth = options.width ?? 400;
        const targetHeight = options.height ?? 300;
        if (options.center === true && options.hasX !== true && options.hasY !== true) {
          const centered = centerBounds({
            left: 0,
            top: 0,
            width: targetWidth,
            height: targetHeight,
          }, parentWindow);
          targetX = centered.left;
          targetY = centered.top;
        }
        subWindow.moveWindowToAsync(targetX, targetY)
          .then(() => hilog.info(DOMAIN, TAG, 'subWindow moveWindowToAsync OK'))
          .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'subWindow moveWindowToAsync failed: %{public}s', JSON.stringify(e)));
        subWindow.resizeAsync(targetWidth, targetHeight)
          .then(() => hilog.info(DOMAIN, TAG, 'subWindow resizeAsync OK'))
          .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'subWindow resizeAsync failed: %{public}s', JSON.stringify(e)));

        // Always on top.
        if (options.alwaysOnTop) {
          subWindow.setWindowTopmost(true)
            .then(() => hilog.info(DOMAIN, TAG, 'subWindow setWindowTopmost OK'))
            .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'subWindow setWindowTopmost failed: %{public}s', JSON.stringify(e)));
        }

        // Initial fullscreen.
        if (options.fullscreen) {
          subWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE)
            .then(() => hilog.info(DOMAIN, TAG, 'subWindow enter-full-screen OK'))
            .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'subWindow enter-full-screen failed: %{public}s', JSON.stringify(e)));
        }

        // Size constraints.
        if (options.minWidth || options.minHeight || options.maxWidth || options.maxHeight) {
          try {
            subWindow.setWindowLimits({
              minWidth: options.minWidth ?? 0,
              minHeight: options.minHeight ?? 0,
              maxWidth: options.maxWidth ?? 0,
              maxHeight: options.maxHeight ?? 0,
            })
              .then(() => hilog.info(DOMAIN, TAG, 'subWindow setWindowLimits OK'))
              .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'subWindow setWindowLimits failed: %{public}s', JSON.stringify(e)));
          } catch (e) {
            hilog.error(DOMAIN, TAG, 'subWindow setWindowLimits failed: %{public}s', String(e));
          }
        }

        // A child must be focusable when its XComponent receives a click;
        // otherwise keyboard and IME input can only reach the parent window.
        try {
          subWindow.keepKeyboardOnFocus(true);
        } catch (e) {
          hilog.error(DOMAIN, TAG, 'subWindow keepKeyboardOnFocus failed: %{public}s', String(e));
        }
        const focusable = options.focusable ?? true;
        subWindow.setWindowFocusable(focusable)
          .then(() => hilog.info(DOMAIN, TAG,
            'subWindow setWindowFocusable(%{public}s) OK', String(focusable)))
          .catch((e: BusinessError) => hilog.error(DOMAIN, TAG,
            'subWindow setWindowFocusable(%{public}s) failed: %{public}s',
            String(focusable), JSON.stringify(e)));

        // Bind C++ window id to the subwindow's HarmonyOS window id.
        lynxtron.setWindowIdForWindow(options.windowId, originWindowId);

        // Register window operation callbacks (minimize/restore/show/close/etc.).
        // Subwindows do not provide Ability-level callbacks; close uses
        // destroyWindow and show/hide use window.Window verbs.
        WindowOpAdapter.registerWindowOpCallbackForWindow(proxy, {});

        // Load UI content and show.
        subWindow.setUIContent('pages/Index', (err) => {
          if (err?.code) {
            hilog.error(DOMAIN, TAG, 'subWindow setUIContent failed: %{public}s', JSON.stringify(err) ?? '');
            return;
          }
          hilog.info(DOMAIN, TAG, 'subWindow setUIContent OK');
          if (options.show !== false) {
            subWindow.showWindow()
              .then(() => {
                hilog.info(DOMAIN, TAG, 'subWindow showWindow OK');
              })
              .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'subWindow showWindow failed: %{public}s', JSON.stringify(e)));
          }
        });
      })
      .catch((err: BusinessError) => {
        hilog.error(DOMAIN, TAG, 'createSubWindowWithOptions failed: %{public}s', JSON.stringify(err) ?? '');
      });
  }

  // Shows the subwindow identified by its C++ window id.
  static showSubWindow(cppWindowId: number): void {
    const proxy = AbilityManager.getByCppWindowId(cppWindowId);
    if (proxy) {
      proxy.getWindow().showWindow()
        .then(() => hilog.info(DOMAIN, TAG, 'showSubWindow OK id=%{public}d', cppWindowId))
        .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'showSubWindow failed: %{public}s', JSON.stringify(e)));
    }
  }

  // Hides the subwindow by minimizing it (subwindows have no separate hide API).
  static hideSubWindow(cppWindowId: number): void {
    const proxy = AbilityManager.getByCppWindowId(cppWindowId);
    if (proxy) {
      proxy.getWindow().minimize()
        .then(() => hilog.info(DOMAIN, TAG, 'hideSubWindow OK id=%{public}d', cppWindowId))
        .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'hideSubWindow failed: %{public}s', JSON.stringify(e)));
    }
  }

  // Closes the subwindow by destroying its OS window.
  static cancelSubWindow(cppWindowId: number): void {
    const proxy = AbilityManager.getByCppWindowId(cppWindowId);
    if (proxy) {
      proxy.getWindow().destroyWindow()
        .then(() => hilog.info(DOMAIN, TAG, 'cancelSubWindow OK id=%{public}d', cppWindowId))
        .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'cancelSubWindow failed: %{public}s', JSON.stringify(e)));
    }
  }

  // Moves and resizes the subwindow to the given bounds.
  static setSubWindowBounds(cppWindowId: number, bounds: WindowBounds): void {
    const proxy = AbilityManager.getByCppWindowId(cppWindowId);
    if (proxy) {
      const win = proxy.getWindow();
      win.moveWindowToAsync(bounds.left, bounds.top)
        .then(() => win.resizeAsync(bounds.width, bounds.height))
        .then(() => hilog.info(DOMAIN, TAG, 'setSubWindowBounds OK id=%{public}d', cppWindowId))
        .catch((e: BusinessError) => hilog.error(DOMAIN, TAG, 'setSubWindowBounds failed: %{public}s', JSON.stringify(e)));
    }
  }
}
