import hilog from '@ohos.hilog';
import window from '@ohos.window';
import lynxtron from 'liblynxtron_napi.so';

const DOMAIN = 0x0000;
const TAG = 'WindowUtils';

// Reports the actual OS window rect (windowRect) to the C++ layer. The C++
// layer keeps this separate from bounds_, which tracks the drawable/content
// rect updated by XComponent surface callbacks.
export function reportWindowRect(windowId: number, win: window.Window): void {
  try {
    const rect = win.getWindowProperties().windowRect;
    if (!rect) {
      return;
    }
    lynxtron.notifyWindowRect(
      windowId,
      rect.left ?? 0,
      rect.top ?? 0,
      rect.width ?? 0,
      rect.height ?? 0
    );
  } catch (e) {
    hilog.error(DOMAIN, TAG, 'reportWindowRect failed: %{public}s', String(e));
  }
}
