import { StatusBarViewExtensionAbility } from '@kit.StatusBarExtensionKit';
import { UIExtensionContentSession, Want } from '@kit.AbilityKit';
import hilog from '@ohos.hilog';

const TAG = 'StatusBarEntryAbility';

export default class StatusBarEntryAbility extends StatusBarViewExtensionAbility {
  onCreate() {
    hilog.info(0x0000, TAG, 'Ability onCreate');
  }

  onSessionCreate(want: Want, session: UIExtensionContentSession) {
    hilog.info(0x0000, TAG, 'Ability onSessionCreate');
    session.loadContent('pages/StatusBarPage');
  }

  onForeground() {
    hilog.info(0x0000, TAG, 'Ability onForeground');
  }

  onBackground() {
    hilog.info(0x0000, TAG, 'Ability onBackground');
  }

  onSessionDestroy(session: UIExtensionContentSession) {
    hilog.info(0x0000, TAG, 'Ability onSessionDestroy');
  }

  onDestroy() {
    hilog.info(0x0000, TAG, 'Ability onDestroy');
  }
}
