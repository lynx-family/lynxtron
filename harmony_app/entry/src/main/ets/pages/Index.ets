import hilog from '@ohos.hilog';
import common from '@ohos.app.ability.common';
import web_webview from '@ohos.web.webview';
import { NodeController, BuilderNode, FrameNode } from '@ohos.arkui.node';
import { UIContext } from '@ohos.arkui.UIContext';
import { updateManager, productViewManager } from '@kit.AppGalleryKit';
import { Want } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { LynxWindow } from '../components/LynxWindow';
import lynxtron from 'liblynxtron_napi.so';

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

// NodeController that builds the Web demo node so it can be composited
// same-layer via a NodeContainer.
class WebNodeController extends NodeController {
  private webController: web_webview.WebviewController = new web_webview.WebviewController();
  private builderNode: BuilderNode<[web_webview.WebviewController]> | null = null;

  // Builds the Web builder node and returns its FrameNode to the NodeContainer.
  makeNode(uiContext: UIContext): FrameNode | null {
    this.builderNode = new BuilderNode(uiContext);
    this.builderNode.build(wrappedWeb, this.webController);
    return this.builderNode.getFrameNode();
  }
}

const DOMAIN = 0x0000;
const TAG = 'LynxtronETS';
// Update callbacks are registered in the NAPI bridge and are process-global.
// Guard the registration when more than one LynxWindow page exists.
const UPDATE_TSFN_REGISTERED_KEY = 'lynxtron_update_tsfn_registered';

@Entry
@Component
struct Index {
  // Main page: hosts the LynxWindow native surface; an optional same-layer
  // Web overlay demo is kept disabled by default.
  // Keep the same-layer Web demo available, but do not place it above the
  // native surface while exercising a Lynx input control: it would receive
  // the touch before XComponent can focus the Lynx field.
  @State showWebOverlay: boolean = false;
  private webNodeController: WebNodeController = new WebNodeController();

  aboutToAppear(): void {
    hilog.info(DOMAIN, TAG, 'Index aboutToAppear');
    // Register TSFN callback for AppGallery Kit requests from Lynx JS.
    // Node.js pushes requests via napi_threadsafe_function — no polling.
    if (AppStorage.get<boolean>(UPDATE_TSFN_REGISTERED_KEY) !== true) {
      AppStorage.setOrCreate(UPDATE_TSFN_REGISTERED_KEY, true);
      hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] aboutToAppear — registering TSFN callback');
      this.registerUpdateCallback();
    } else {
      hilog.info(DOMAIN, TAG, '[UpdateModel Index.ets] reuse process-global TSFN callback');
    }
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

  // Handles a TSFN update-check request from Lynx JS via AppGalleryKit.
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

  // Handles a TSFN show-update-dialog request from Lynx JS via AppGalleryKit.
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

  // Handles a TSFN load-product request from Lynx JS via AppGalleryKit.
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

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      // Native Lynxtron rendering surface. XComponent id resolution, IME sync
      // and lynxtron.start() trigger now live inside the LynxWindow component.
      LynxWindow()

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
