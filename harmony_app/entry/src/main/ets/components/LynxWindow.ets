import lynxtron from 'liblynxtron_napi.so';
import hilog from '@ohos.hilog';
import window from '@ohos.window';
import common from '@ohos.app.ability.common';
import { AbilityManager } from '../common/AbilityManager';

const DOMAIN = 0x0000;
const TAG = 'LynxWindow';

const LYNXTRON_STARTED_KEY = 'lynxtron_started';
// The native IME bridge is process-global. Each ArkTS page owns a LynxWindow
// component, so start one poller for the process rather than one per window.
const IME_SYNC_TIMER_STARTED_KEY = 'lynxtron_ime_sync_timer_started';

// Formats an unknown error object into a compact diagnostic string.
function describeError(e: Object): string {
  const err = e as Record<string, Object>;
  const parts: string[] = [];
  if (err) {
    if (err.name !== undefined) parts.push(String(err.name));
    if (err.code !== undefined) parts.push('code=' + String(err.code));
    if (err.message !== undefined) parts.push(String(err.message));
  }
  if (parts.length === 0) {
    parts.push(String(e));
  }
  return parts.join(' | ');
}

// Dumps the lynxtron module's type and exported keys for diagnostics.
function inspectModule(): string {
  if (typeof lynxtron === 'undefined' || lynxtron === null) {
    return 'UNDEFINED/NULL';
  }
  const mod = lynxtron as Record<string, Object>;
  const keys = Object.keys(mod);
  const types: string[] = [];
  for (const k of keys) {
    types.push(k + ':' + typeof mod[k]);
  }
  return 'typeof=' + typeof lynxtron + ' keys=[' + keys.join(',') + '] ' + types.join(',');
}

@Component
export struct LynxWindow {
  @State xcomponentId: string = 'lynxtron_surface_-1';

  aboutToAppear(): void {
    let originWindowId = -1;
    let via = 'none';
    let windowName = '';
    try {
      windowName = this.getUIContext().getWindowName() ?? '';
      hilog.info(DOMAIN, TAG, 'aboutToAppear windowName=%{public}s', windowName);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'getWindowName failed: %{public}s', String(e));
    }

    // A subwindow shares the parent Ability's context, so context-based lookup
    // below would resolve to the parent window and route the subwindow's
    // XComponent surface (and every size change produced while dragging it) to
    // the parent window, corrupting the parent's layout. Subwindows are created
    // with a unique name ("lynxtron-sub-<cppWindowId>") in
    // SubWindowAdapter.createSubWindow; resolve the subwindow's own harmony
    // window id from that name first.
    if (windowName.startsWith('lynxtron-sub-')) {
      try {
        const cppWindowId = Number(windowName.substring('lynxtron-sub-'.length));
        if (Number.isInteger(cppWindowId) && cppWindowId > 0) {
          const proxy = AbilityManager.getByCppWindowId(cppWindowId);
          if (proxy && proxy.getOriginWindowId() > 0) {
            originWindowId = proxy.getOriginWindowId();
            via = 'sub-name';
          }
        }
        if (originWindowId <= 0) {
          // Registration may race the page load in rare cases; fall back to the
          // window manager's name lookup.
          const win = window.findWindow(windowName);
          originWindowId = win.getWindowProperties().id;
          via = 'sub-findWindow';
        }
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'resolve subwindow origin id failed: %{public}s', String(e));
      }
    }

    // Main window: first try the exact Ability context that BaseWindowAbility
    // registered (semantically "this ability's own window").
    if (originWindowId <= 0) {
      try {
        const ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
        const proxy = AbilityManager.getByContext(ctx);
        if (proxy && proxy.getOriginWindowId() > 0) {
          originWindowId = proxy.getOriginWindowId();
          via = 'context';
        }
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'resolve origin window id failed: %{public}s', String(e));
      }
    }

    // Main window fallback: resolve by the window manager's unique window name.
    // window.findWindow(name) is synchronous and does not depend on context
    // object identity, unlike the context lookup above.
    if (originWindowId <= 0 && windowName.length > 0) {
      try {
        const win = window.findWindow(windowName);
        originWindowId = win.getWindowProperties().id;
        via = 'main-findWindow';
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'findWindow(%{public}s) failed: %{public}s', windowName, String(e));
      }
    }

    // Do NOT fall back to window.getLastWindow(): it returns the topmost
    // window, so in multi-window every page would read the same id and bind
    // to the same XComponent. Leaving -1 makes the native side use its
    // thread-local harmony window id, which is correct because each main
    // window (Ability) runs on its own thread.
    if (originWindowId <= 0) {
      originWindowId = -1;
      via = 'tls-fallback';
    }
    this.xcomponentId = 'lynxtron_surface_' + originWindowId;
    hilog.info(DOMAIN, TAG, 'xcomponentId=%{public}s via=%{public}s', this.xcomponentId, via);

    // Text input is owned entirely by the native side: the bridge registers an
    // InputMethod_TextEditorProxy (inputmethod NDK) and becomes the system
    // IME's text client directly, so no ArkUI TextInput is involved and nothing
    // steals focus from XComponent. This tick only lets the bridge observe
    // Lynx's focus/caret state from the ArkUI thread and attach, detach, or
    // move the candidate window accordingly.
    if (AppStorage.get<boolean>(IME_SYNC_TIMER_STARTED_KEY) !== true) {
      AppStorage.setOrCreate(IME_SYNC_TIMER_STARTED_KEY, true);
      setInterval(() => this.syncImeState(), 50);
    } else {
      hilog.info(DOMAIN, TAG, 'reuse process-global IME sync timer');
    }
  }

  // Ticks the native IME bridge so it can attach/detach/move the candidate
  // window from the ArkUI thread.
  private syncImeState(): void {
    const mod = lynxtron as Record<string, Object>;
    if (!mod || typeof mod.syncIme !== 'function') return;
    lynxtron.syncIme();
  }

  // Invokes the one-shot native lynxtron.start() with module diagnostics.
  private startLynxtron(): void {
    if (typeof lynxtron === 'undefined' || lynxtron === null) {
      hilog.error(DOMAIN, TAG, 'lynxtron module is undefined/null');
      return;
    }
    const mod = lynxtron as Record<string, Object>;
    if (typeof mod.start !== 'function') {
      hilog.error(DOMAIN, TAG, 'lynxtron.start is not a function: %{public}s', inspectModule());
      return;
    }
    hilog.info(DOMAIN, TAG, '>>> calling lynxtron.start()');
    try {
      const rc: number = lynxtron.start();
      hilog.info(DOMAIN, TAG, '<<< lynxtron.start() returned rc=%{public}d', rc);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'lynxtron.start() threw: %{public}s', describeError(e as Object));
    }
  }

  build() {
    XComponent({
      id: this.xcomponentId,
      type: XComponentType.SURFACE,
      libraryname: 'lynxtron_napi'
    })
      .width('100%')
      .height('100%')
      .onLoad(() => {
        hilog.info(DOMAIN, TAG, 'XComponent onLoad — surface ready');
        // LynxtronMain is started once globally. The C++ side uses std::call_once
        // as a safety net; the ArkTS guard avoids duplicate logical triggers.
        if (!AppStorage.get<boolean>(LYNXTRON_STARTED_KEY)) {
          AppStorage.setOrCreate(LYNXTRON_STARTED_KEY, true);
          this.startLynxtron();
        }
      })
      // NativeXComponent only dispatches registered hardware-key callbacks
      // while its ArkUI node owns focus. A mouse click must therefore move
      // focus to this surface before the Lynx input receives number keys.
      .focusable(true)
      .focusOnTouch(true)
  }
}
