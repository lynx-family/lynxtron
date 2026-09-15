import common from '@ohos.app.ability.common';
import { statusBarManager } from '@kit.StatusBarExtensionKit';
import { image } from '@kit.ImageKit';
import hilog from '@ohos.hilog';

const DOMAIN = 0x0000;
const TAG = 'StatusBarManager';

export class StatusBarManager {
  private static isInitialized = false;

  // Idempotently builds the status-bar icons and attaches the status-bar item
  // that hosts the app in the system status bar.
  static init(context: common.UIAbilityContext): Promise<void> {
    if (StatusBarManager.isInitialized) {
      return Promise.resolve();
    }

    return new Promise<void>((resolve, reject) => {
      Promise.all([
        StatusBarManager.createStatusBarIcon('white'),
        StatusBarManager.createStatusBarIcon('black'),
      ])
        .then((pixelMaps: (image.PixelMap | undefined)[]) => {
          const white = pixelMaps[0];
          const black = pixelMaps[1];
          if (!white || !black) {
            const msg = 'failed to create status bar icons';
            hilog.error(DOMAIN, TAG, msg);
            reject(new Error(msg));
            return;
          }

          try {
            hilog.info(DOMAIN, TAG, 'calling addToStatusBar');
            // The sync call only submits the request; the icon is attached
            // asynchronously and the framework's completion callback is
            // unreliable. Readiness is probed by retrying startAbility()
            // (see AppWindowAdapter), so do not wait here.
            statusBarManager.addToStatusBar(context, {
              icons: { white, black },
              quickOperation: {
                title: 'Lynxtron',
                height: 30,
                moduleName: 'entry',
                abilityName: 'StatusBarEntryAbility',
              },
            });
            StatusBarManager.isInitialized = true;
            hilog.info(DOMAIN, TAG, 'addToStatusBar submitted');
            resolve();
          } catch (e) {
            hilog.error(DOMAIN, TAG, 'addToStatusBar threw: %{public}s', String(e));
            reject(e);
          }
        })
        .catch((e: Error) => {
          hilog.error(DOMAIN, TAG, 'loadAppIconPixelMap failed: %{public}s', String(e));
          reject(e);
        });
    });
  }

  // Build a tiny status-bar icon in-memory. The system rejects the
  // raw app.media.icon (usually 512x512) with "pixelmap exceeds the limit".
  private static async createStatusBarIcon(color: 'white' | 'black'): Promise<image.PixelMap | undefined> {
    // Use a 1x1 pixelmap as a sanity check: if even this is rejected, the
    // problem is not the icon dimensions but the API usage or device support.
    const width = 1;
    const height = 1;
    const buffer = new ArrayBuffer(width * height * 4);
    const pixels = new Uint8Array(buffer);
    // StatusBarIcon.white should be pure white, black should be pure black.
    const value = color === 'white' ? 0xff : 0x00;
    pixels[0] = value; // R
    pixels[1] = value; // G
    pixels[2] = value; // B
    pixels[3] = 0xff;  // A
    try {
      const pixelMap = await image.createPixelMap(buffer, {
        pixelFormat: image.PixelMapFormat.RGBA_8888,
        size: { width, height },
        editable: false,
      });
      const info = await pixelMap.getImageInfo();
      hilog.info(DOMAIN, TAG, 'created %{public}s icon %{public}dx%{public}d format=%{public}d',
        color, info.size.width, info.size.height, info.pixelFormat);
      return pixelMap;
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'createStatusBarIcon failed: %{public}s', String(e));
      return undefined;
    }
  }
}
