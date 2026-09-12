import Want from '@ohos.app.ability.Want';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import { BusinessError } from '@ohos.base';
import hilog from '@ohos.hilog';
import window from '@ohos.window';
import { StatusBarManager } from '../manager/StatusBarManager';
import LogUtil from '../utils/LogUtils';
import AppAdapter from '../adapter/AppAdapter';
import lynxtron from 'liblynxtron_napi.so';
import { BridgeRegistry } from '../bridge/BridgeRegistry';
import BaseWindowAbility from '../ability/BaseWindowAbility';

const DOMAIN = 0x0000;
const TAG = 'Lynxtron';

// Logs all parameters carried by a Want for diagnostics.
function dumpWantParameters(want: Want): void {
  const params = want.parameters;
  if (!params) {
    hilog.warn(DOMAIN, TAG, 'want.parameters is null/undefined');
    return;
  }
  const dict = params as Record<string, Object>;
  const keys = Object.keys(dict);
  hilog.info(DOMAIN, TAG, 'want.parameters keys=[%{public}s]', keys.join(','));
  for (const k of keys) {
    hilog.info(DOMAIN, TAG, '  %{public}s = %{public}s', k, String(dict[k]));
  }
}

export default class EntryAbility extends BaseWindowAbility {
  // Parses debug launch params and stores the global Ability context.
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG,
      'EntryAbility onCreate launchReason=%{public}d lastExitReason=%{public}d',
      launchParam.launchReason, launchParam.lastExitReason);

    dumpWantParameters(want);

    const params = want.parameters as Record<string, Object> | undefined;
    const debug = params?.['electron_debug'];
    if (debug !== undefined) {
      const value = String(debug);
      hilog.info(DOMAIN, TAG, '>>> electron_debug=%{public}s detected', value);
      AppStorage.setOrCreate('electron_debug', value);
    } else {
      hilog.info(DOMAIN, TAG, '(no electron_debug in launch params)');
    }

    AppStorage.setOrCreate('abilityContext', this.context);
    AppAdapter.getInstance().setAbilityContext(this.context);

    this.handleWantUri(want);
  }

  // Handles a new Want delivered to the running EntryAbility (open-url/open-file).
  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, TAG,
      'EntryAbility onNewWant launchReason=%{public}d', launchParam.launchReason);
    dumpWantParameters(want);
    LogUtil.info(TAG,
      `[ohoswindow] in WebAbility onNewWant, want is ${JSON.stringify(want)}, param is ${JSON.stringify(launchParam)}.`);
    this.handleWantUri(want);
  }

  // Routes a file:// or custom-scheme URI from the Want into AppAdapter.
  private handleWantUri(want: Want): void {
    let uriNotNull = want["uri"] !== undefined && want["uri"] !== '';
    if (!uriNotNull) {
      return;
    }
    let uri = want['uri'] as string;
    if (uri.startsWith("file://docs/")) {
      AppAdapter.getInstance().openNewWindow(uri.replace("file://docs/", "file:///"));
    } else if (uri.startsWith("file:///")) {
      AppAdapter.getInstance().openNewWindow(uri);
    } else {
      LogUtil.debug(TAG, `Trigger open-url event. uri = ${uri}`);
      AppAdapter.getInstance().openURL(uri);
    }
  }

  onDestroy(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onDestroy');
    // System/ArkTS side wants to quit: route through the graceful native path
    // (Application::Quit, same as JS app.quit()) rather than force-killing the
    // detached LynxtronMain thread. Mirrors electron_ohos's kAppQuit command.
    AppAdapter.getInstance().quit();
  }

  async onWindowStageCreate(windowStage: window.WindowStage): Promise<void> {
    hilog.info(DOMAIN, TAG, 'EntryAbility onWindowStageCreate');

    // EntryAbility is the first business window. Publish its context, register
    // bridges and the status-bar item, then let BaseWindowAbility register the
    // placeholder proxy and load the page.
    AppStorage.setOrCreate('active_ability_context', this.context);
    BridgeRegistry.registerAll(this.context);
    StatusBarManager.init(this.context)
      .then(() => {
        hilog.info(DOMAIN, TAG, 'StatusBarManager.init OK');
      })
      .catch((e: BusinessError) => {
        hilog.error(DOMAIN, TAG, 'StatusBarManager.init failed: %{public}s', String(e));
      });

    const mainWindow = windowStage.getMainWindowSync();
    try {
      const props = mainWindow.getWindowProperties();
      const displayId = props.displayId;
      AppStorage.setOrCreate('entry_ability_display_id', displayId);
      hilog.info(DOMAIN, TAG, 'EntryAbility displayId=%{public}d', displayId);
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'EntryAbility get displayId failed: %{public}s', String(e));
    }

    // Store the main window object for native window-decor commands. This is
    // global and only set once, by the entry window.
    lynxtron.setWindowObject(mainWindow);

    // BaseWindowAbility registers the placeholder proxy (cppWindowId=0), binds
    // lifecycle listeners, registers the window-op callback, and calls
    // onWindowCreated to load pages/Index. lynxtron.start() is triggered from
    // the LynxWindow component's XComponent.onLoad.
    super.onWindowStageCreate(windowStage);
  }

  // Loads pages/Index into the entry window; decoration options are applied
  // later through first-window binding.
  onWindowCreated(windowStage: window.WindowStage, mainWindow: window.Window): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onWindowCreated');
    mainWindow.setUIContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, 'setUIContent failed: %{public}s',
          JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(DOMAIN, TAG, 'setUIContent OK');
    });
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onWindowStageDestroy');
  }

  onForeground(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onForeground');
  }

  onBackground(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onBackground');
  }
}
