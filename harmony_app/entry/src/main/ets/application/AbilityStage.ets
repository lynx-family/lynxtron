import AbilityStage from '@ohos.app.ability.AbilityStage';
import Want from '@ohos.app.ability.Want';
import hilog from '@ohos.hilog';
import lynxtron from 'liblynxtron_napi.so';
import { AppWindowAdapter } from '../adapter/AppWindowAdapter';

const DOMAIN = 0x0000;
const TAG = 'LynxtronAbilityStage';

export default class LynxtronAbilityStage extends AbilityStage {
  onCreate(): void {
    hilog.info(DOMAIN, TAG, 'onCreate');

    // Register the C++ → ArkTS window creation bridge. When JS does
    // `new BaseWindow(options)`, C++ forwards the request to this callback.
    try {
      lynxtron.registerCreateWindowCallback(AppWindowAdapter.createWindowFromCpp.bind(AppWindowAdapter));
      hilog.info(DOMAIN, TAG, 'registerCreateWindowCallback OK');
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'registerCreateWindowCallback failed: %{public}s', String(e));
    }
  }

  // launchType: specified requires onAcceptWant to return the instance key so
  // that the system can create or reuse the matching Ability instance.
  onAcceptWant(want: Want): string {
    const instanceKey = want.parameters?.['instanceKey'];
    if (instanceKey !== undefined) {
      return String(instanceKey);
    }
    hilog.warn(DOMAIN, TAG, 'onAcceptWant fallback to default instance');
    return 'default';
  }
}
