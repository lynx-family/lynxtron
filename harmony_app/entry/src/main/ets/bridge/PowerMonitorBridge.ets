import common from '@ohos.app.ability.common';
import commonEventManager from '@ohos.commonEventManager';
import hilog from '@ohos.hilog';
import lynxtron from 'liblynxtron_napi.so';

const DOMAIN = 0x0000;
const TAG = 'PowerMonitorBridge';

export class PowerMonitorBridge {
  private static instance: PowerMonitorBridge | null = null;
  private subscriber: commonEventManager.CommonEventSubscriber;

  private constructor() {
    this.subscriber = commonEventManager.createSubscriberSync({
      events: [
        commonEventManager.Support.COMMON_EVENT_SCREEN_LOCKED,
        commonEventManager.Support.COMMON_EVENT_SCREEN_UNLOCKED
      ]
    });
  }

  static register(context: common.UIAbilityContext): void {
    if (PowerMonitorBridge.instance) {
      hilog.warn(DOMAIN, TAG, 'Already registered, skipping');
      return;
    }

    const bridge = new PowerMonitorBridge();
    PowerMonitorBridge.instance = bridge;
    commonEventManager.subscribe(bridge.subscriber, (err, data) => {
      if (err) {
        hilog.error(DOMAIN, TAG, 'subscribe failed: %{public}s',
          JSON.stringify(err));
        return;
      }
      if (data.event === commonEventManager.Support.COMMON_EVENT_SCREEN_LOCKED) {
        lynxtron.notifyPowerMonitorLockScreen();
        hilog.info(DOMAIN, TAG, 'lock-screen dispatched');
      } else if (data.event ===
        commonEventManager.Support.COMMON_EVENT_SCREEN_UNLOCKED) {
        lynxtron.notifyPowerMonitorUnlockScreen();
        hilog.info(DOMAIN, TAG, 'unlock-screen dispatched');
      }
    });
  }
}