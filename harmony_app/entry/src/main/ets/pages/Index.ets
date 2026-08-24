import lynxtron from 'liblynxtron_napi.so';
import hilog from '@ohos.hilog';
import window from '@ohos.window';
import common from '@ohos.app.ability.common';
import { AbilityManager } from '../common/AbilityManager';
import web_webview from '@ohos.web.webview';
import { NodeController, BuilderNode, FrameNode } from '@ohos.arkui.node';
import { UIContext } from '@ohos.arkui.UIContext';
import { updateManager, productViewManager } from '@kit.AppGalleryKit';
import { Want } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';

// ---- Same-layer rendering (official ArkUI PlatformView-style embedding) ----
// A @Builder that builds the real ArkWeb Web node. NodeController.makeNode()
// builds it into a BuilderNode and hands its FrameNode to a NodeContainer, which
// composites it same-layer with the Clay XComponent. This is the HarmonyOS
// blessed way for a third-party UI framework to embed native components
// (Web/Video/TextInput/Canvas/XComponent), superior to a plain Stack overlay.
@Builder
function buildWeb(controller: web_webview.WebviewController) {
  Web({
    src: "data:text/html,<body style='margin:0;background:%23107c41;font-family:sans-serif'><h1 style='color:white;padding:20px'>Same-layer Web via NodeContainer</h1><p style='color:%23d6ffd6;padding:0 20px;font-size:18px'>Built by NodeController.makeNode() -&gt; BuilderNode -&gt; FrameNode, hosted in a NodeContainer (official ArkUI same-layer rendering).</p></body>",
    controller: controller
  })
    .width('100%')
    .height('100%')
}

const wrappedWeb: WrappedBuilder<[web_webview.WebviewController]> = wrapBuilder(buildWeb);

class WebNodeController extends NodeController {
  private webController: web_webview.WebviewController = new web_webview.WebviewController();
  private builderNode: BuilderNode<[web_webview.WebviewController]> | null = null;

  makeNode(uiContext: UIContext): FrameNode | null {
    this.builderNode = new BuilderNode(uiContext);
    this.builderNode.build(wrappedWeb, this.webController);
    return this.builderNode.getFrameNode();
  }
}

const DOMAIN = 0x0000;
const TAG = 'LynxtronETS';

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

const moduleDesc = inspectModule();
hilog.info(DOMAIN, TAG, '=== Module load diagnostics ===');
hilog.info(DOMAIN, TAG, 'typeof lynxtron = %{public}s', typeof lynxtron);
hilog.info(DOMAIN, TAG, 'inspect: %{public}s', moduleDesc);
if (typeof lynxtron !== 'undefined' && lynxtron !== null) {
  hilog.info(DOMAIN, TAG, 'typeof lynxtron.start = %{public}s', typeof (lynxtron as Record<string, Object>).start);
}

@Entry
@Component
struct Index {
  @State message: string = 'Lynxtron HarmonyOS bring-up';
  @State lastResult: string = '(not started yet)';
  @State moduleInfo: string = moduleDesc;
  @State logLines: string[] = [];
  @State electronDebug: string = AppStorage.get<string>('electron_debug') ?? '(not set)';
  // Keep the same-layer Web demo available, but do not place it above the
  // native surface while exercising a Lynx input control: it would receive
  // the touch before XComponent can focus the Lynx field.
  @State showWebOverlay: boolean = false;
  @State xcomponentId: string = 'lynxtron_surface_-1';
  private webNodeController: WebNodeController = new WebNodeController();

  aboutToAppear(): void {
    // LynxtronMain is now started from XComponent.onLoad (see build()), so the
    // native surface exists before the render pipeline spins up.
    hilog.info(DOMAIN, TAG, 'aboutToAppear — waiting for XComponent surface');
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
    // AppWindowAdapter.createSubWindowFromCpp; resolve the subwindow's own
    // harmony window id from that name first.
    if (windowName.startsWith('lynxtron-sub-')) {
      try {
        const cppWindowId = Number(windowName.substring('lynxtron-sub-'.length));
        if (Number.isInteger(cppWindowId) && cppWindowId > 0) {
          const proxy = AbilityManager.getByCppWindowId(cppWindowId);
          if (proxy && proxy.originWindowId > 0) {
            originWindowId = proxy.originWindowId;
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
        if (proxy && proxy.originWindowId > 0) {
          originWindowId = proxy.originWindowId;
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
    hilog.info(DOMAIN, TAG, 'xcomponentId=%{public}s via=%{public}s',
      this.xcomponentId, via);
    // Text input is owned entirely by the native side: the bridge registers an
    // InputMethod_TextEditorProxy (inputmethod NDK) and becomes the system
    // IME's text client directly, so no ArkUI TextInput is involved and nothing
    // steals focus from XComponent. This tick only lets the bridge observe
    // Lynx's focus/caret state from the ArkUI thread and attach, detach, or
    // move the candidate window accordingly.
    setInterval(() => this.syncImeState(), 50);
    // Register TSFN callback for AppGallery Kit requests from Lynx JS.
    // Node.js pushes requests via napi_threadsafe_function — no polling.
    hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] aboutToAppear — registering TSFN callback');
    this.registerUpdateCallback();
  }

  syncImeState(): void {
    const mod = lynxtron as Record<string, Object>;
    if (!mod || typeof mod.syncIme !== 'function') return;
    lynxtron.syncIme();
  }

  // ---- AppGallery Kit: TSFN callback (replaces polling) ----

  registerUpdateCallback(): void {
    const mod = lynxtron as Record<string, Object>;
    if (!mod || typeof mod.registerUpdateTSFN !== 'function') {
      hilog.warn(DOMAIN, TAG, '[UpdateModel Index.ets] registerUpdateTSFN not available');
      return;
    }
    lynxtron.registerUpdateTSFN((type: number) => {
      hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] TSFN callback: type=%{public}d', type);
      switch (type) {
        case 0: this.handleCheckAppUpdate(); break;
        case 1: this.handleShowUpdateDialog(); break;
        case 2: this.handleLoadProduct(); break;
        default: hilog.warn(DOMAIN, TAG, '[UpdateModel Index.ets] unknown TSFN type=%{public}d', type);
      }
    });
    hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] TSFN callback registered');
  }

  handleCheckAppUpdate(): void {
    try {
      const ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
      updateManager.checkAppUpdate(ctx)
        .then((result: updateManager.CheckUpdateResult) => {
          hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] checkAppUpdate OK: %{public}s', String(result.updateAvailable));
          lynxtron.resolveCheckAppUpdate(JSON.stringify(result));
        })
        .catch((error: BusinessError) => {
          hilog.error(DOMAIN, TAG, '[UpdateModel Index.ets] checkAppUpdate FAILED: code=%{public}d', error.code);
          lynxtron.resolveCheckAppUpdate(JSON.stringify({ updateAvailable: false, error: error.message }));
        });
    } catch (e) {
      hilog.error(DOMAIN, TAG, '[UpdateModel Index.ets] checkAppUpdate threw: %{public}s', String(e));
      lynxtron.resolveCheckAppUpdate(JSON.stringify({ updateAvailable: false, error: String(e) }));
    }
  }

  handleShowUpdateDialog(): void {
    hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] showUpdateDialog requested');
    try {
      const ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
      updateManager.showUpdateDialog(ctx)
        .then((resultCode: updateManager.ShowUpdateResultCode) => {
          hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] showUpdateDialog OK: code=%{public}d', resultCode);
          lynxtron.resolveShowUpdateDialog(resultCode);
        })
        .catch((error: BusinessError) => {
          hilog.error(DOMAIN, TAG, '[UpdateModel Index.ets] showUpdateDialog FAILED: code=%{public}d', error.code);
          lynxtron.resolveShowUpdateDialog(-1);
        });
    } catch (e) {
      hilog.error(DOMAIN, TAG, '[UpdateModel Index.ets] showUpdateDialog threw: %{public}s', String(e));
      lynxtron.resolveShowUpdateDialog(-1);
    }
  }

  handleLoadProduct(): void {
    hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] loadProduct requested');
    try {
      const ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
      // Params hardcoded — TSFN only carries a type code, no payload.
      const wantParam: Want = {
        bundleName: 'com.huawei.hmsapp.books',
      };
      const callback: productViewManager.ProductViewCallback = {
        onError: (error: BusinessError) => {
          hilog.error(DOMAIN, TAG, '[UpdateModel Index.ets] loadProduct onError: code=%{public}d', error.code);
          lynxtron.resolveLoadProduct(JSON.stringify({ success: false, errorCode: error.code, errorMessage: error.message }));
        },
        onAppear: () => {
          hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] loadProduct onAppear');
          lynxtron.resolveLoadProduct(JSON.stringify({ success: true }));
        },
        onDisappear: () => {
          hilog.info(DOMAIN, TAG, '[Update] loadProduct onDisappear');
        },
      };
      productViewManager.loadProduct(ctx, wantParam, callback);
    } catch (e) {
      hilog.error(DOMAIN, TAG, '[UpdateModel Index.ets] loadProduct threw: %{public}s', String(e));
      lynxtron.resolveLoadProduct(JSON.stringify({ success: false, errorMessage: String(e) }));
    }
  }

  startLynxtron(): void {
    if (typeof lynxtron === 'undefined' || lynxtron === null) {
      const msg = 'lynxtron module is undefined/null — native .so did not register';
      hilog.error(DOMAIN, TAG, '%{public}s', msg);
      this.lastResult = msg;
      this.addLog('ERR: ' + msg);
      return;
    }

    const mod = lynxtron as Record<string, Object>;
    const startType = typeof mod.start;
    hilog.info(DOMAIN, TAG, 'typeof lynxtron.start = %{public}s', startType);
    this.addLog('typeof .start = ' + startType);

    if (startType !== 'function') {
      const detail = inspectModule();
      const msg = '.start is ' + startType + ', not function. ' + detail;
      hilog.error(DOMAIN, TAG, '%{public}s', msg);
      this.lastResult = msg;
      this.addLog('ERR: ' + msg);
      return;
    }

    hilog.info(DOMAIN, TAG, '>>> calling lynxtron.start() now');
    this.addLog('Calling lynxtron.start()...');

    try {
      const rc: number = lynxtron.start();
      hilog.info(DOMAIN, TAG, '<<< lynxtron.start() returned rc=%{public}d', rc);
      this.lastResult = 'rc=' + rc;
      this.addLog('OK rc=' + rc);
    } catch (e) {
      const desc = describeError(e as Object);
      const err = e as Record<string, Object>;
      hilog.error(DOMAIN, TAG, 'lynxtron.start() threw: %{public}s', desc);
      this.addLog('THROW: ' + desc);

      if (err?.stack !== undefined) {
        const stack = String(err.stack);
        hilog.error(DOMAIN, TAG, 'stack: %{public}s', stack);
        this.addLog('STACK: ' + stack);
      }

      const allKeys = Object.keys(err);
      if (allKeys.length > 0) {
        const dump = allKeys.map((k: string): string => k + '=' + String(err[k])).join('; ');
        hilog.error(DOMAIN, TAG, 'error fields: %{public}s', dump);
        this.addLog('FIELDS: ' + dump);
      }

      this.lastResult = desc;
    }
  }

  addLog(line: string): void {
    this.logLines = [...this.logLines, line];
    if (this.logLines.length > 30) {
      this.logLines = this.logLines.slice(this.logLines.length - 30);
    }
  }

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      Column() {
        // XComponent produces a native surface; its onLoad NAPI callback in
        // lynxtron_napi_bridge.cc receives OH_NativeXComponent and hands the
        // EGL window down to NativeWindowHarmony for Chromium/Lynx to draw into.
        // The id encodes the origin window id so C++ can route surface updates
        // per-window instead of assuming a single window per thread.
        XComponent({
          id: this.xcomponentId,
          type: XComponentType.SURFACE,
          libraryname: 'lynxtron_napi'
        })
          .width('100%')
          .layoutWeight(1)
          .onLoad(() => {
            hilog.info(DOMAIN, TAG, 'XComponent onLoad — surface ready');
            this.startLynxtron();
          })

        Text('Result: ' + this.lastResult)
          .fontSize(11)
          .fontColor('#999999')
          .height(16)
          .onClick(()=>{

          })
      }
      .width('100%')
      .height('100%')

      // Same-layer Web hosted via NodeContainer (official ArkUI mechanism).
      // NodeContainer(this.webNodeController)
      //   .width(1100)
      //   .height(700)
      //   .margin({ top: 120, left: 120 })
      //   .border({ width: 3, color: Color.Red })
    }
    .width('100%')
    .height('100%')
  }
}
