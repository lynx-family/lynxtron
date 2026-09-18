import window from '@ohos.window';
import hilog from '@ohos.hilog';
import BaseWindowAbility from './BaseWindowAbility';

const DOMAIN = 0x0000;
const TAG = 'WindowAbility';

export default class WindowAbility extends BaseWindowAbility {
  // Loads pages/Index into the new window and applies decoration options
  // once the page is ready.
  onWindowCreated(windowStage: window.WindowStage, mainWindow: window.Window): void {
    hilog.info(DOMAIN, TAG, 'onWindowCreated windowId=%{public}d origin=%{public}d',
               this.windowId, this.originWindowId);

    mainWindow.setUIContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, 'setUIContent failed: %{public}s', JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(DOMAIN, TAG, 'setUIContent OK');
      this.applyWindowOptions(mainWindow, windowStage);
    });
  }
}
