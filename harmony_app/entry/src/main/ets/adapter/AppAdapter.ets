import lynxtron from 'liblynxtron_napi.so';
import { common } from '@kit.AbilityKit';
import LogUtil from '../utils/LogUtils';

const TAG = 'AppAdapter';

class AppAdapter {
  private static instance: AppAdapter;
  private context: common.UIAbilityContext | null = null;

  static getInstance(): AppAdapter {
    if (!AppAdapter.instance) {
      AppAdapter.instance = new AppAdapter();
    }
    return AppAdapter.instance;
  }

  setAbilityContext(context: common.UIAbilityContext): void {
    this.context = context;
    lynxtron.setAbilityContext(context);
    LogUtil.info(TAG, 'setAbilityContext done');
  }

  openURL(url: string): void {
    LogUtil.info(TAG, 'openURL: ' + url);
    lynxtron.openUrl(url);
  }

  openNewWindow(openUrl: string): void {
    LogUtil.info(TAG, 'openNewWindow: ' + openUrl);
    lynxtron.openPath(openUrl);
  }

  quit(): void {
    LogUtil.info(TAG, 'quit()');
    lynxtron.quit();
  }
}

export default AppAdapter;
