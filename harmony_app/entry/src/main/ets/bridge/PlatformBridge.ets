import { OpenLinkOptions, Want, wantConstant } from '@kit.AbilityKit';
import common from '@ohos.app.ability.common';
import fileUri from '@ohos.file.fileuri';
import hilog from '@ohos.hilog';
import lynxtron from 'liblynxtron_napi.so';

const DOMAIN = 0x0000;
const TAG = 'PlatformBridge';

/**
 * Bridges platform_util calls (C++) to OHOS platform APIs.
 *
 * Electron reference: FileManagerAdapter in chromium-electron-release.
 *
 * openExternal: HTTP → want { entities: ['entity.system.browsable'] }
 *               Other → want { uri } (system routes by scheme)
 * openPath:     dir  → openLink('filemanager://openDirectory')
 *               file → want { viewData, AUTH_READ|AUTH_WRITE }
 */
export class PlatformBridge {
  private static instance: PlatformBridge | null = null;
  private ctx: common.UIAbilityContext;

  private constructor(ctx: common.UIAbilityContext) {
    this.ctx = ctx;
  }

  /**
   * Register all platform handlers with the lynxtron NAPI bridge.
   * Call once from EntryAbility.onWindowStageCreate.
   */
  static register(context: common.UIAbilityContext): void {
    if (PlatformBridge.instance) {
      hilog.warn(DOMAIN, TAG, 'Already registered, skipping');
      return;
    }
    PlatformBridge.instance = new PlatformBridge(context);
    const bridge = PlatformBridge.instance;

    lynxtron.registerOpenExternal((url: string) => {
      bridge.handleUrl(url);
    });

    lynxtron.registerOpenPath((path: string, isDirectory: number) => {
      bridge.handlePath(path, isDirectory !== 0);
    });

    hilog.info(DOMAIN, TAG, 'Registered openExternal + openPath handlers');
  }

  // ===== openExternal =====

  private handleUrl(url: string): void {
    hilog.info(DOMAIN, TAG, 'handleUrl: %{public}s', url);
    if (!url) return;
    if (url.startsWith('http://') || url.startsWith('https://')) {
      this.openUrlInBrowser(url);
    } else {
      this.openExternal(url);
    }
  }

  private openUrlInBrowser(url: string): void {
    const want: Want = {
      action: 'ohos.want.action.viewData',
      entities: ['entity.system.browsable'],
      uri: url
    };
    try {
      this.ctx.startAbility(want);
      hilog.info(DOMAIN, TAG, 'openUrlInBrowser OK: %{public}s', url);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'openUrlInBrowser failed: %{public}s', JSON.stringify(e));
    }
  }

  private openExternal(url: string): void {
    const want: Want = {
      action: 'ohos.want.action.viewData',
      uri: url
    };
    try {
      this.ctx.startAbility(want);
      hilog.info(DOMAIN, TAG, 'openExternal OK: %{public}s', url);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'openExternal failed: %{public}s', JSON.stringify(e));
    }
  }

  // ===== openPath =====

  private handlePath(path: string, isDirectory: boolean): void {
    hilog.info(DOMAIN, TAG, 'handlePath: %{public}s isDir=%{public}s', path, String(isDirectory));
    if (!path) return;
    if (isDirectory) {
      this.openItemInFolder(path);
    } else {
      this.openVerifiedItem(path);
    }
  }

  /**
   * Open a directory in the system file manager.
   * Electron: FileManagerAdapter.openItemInFolder
   */
  private openItemInFolder(filePath: string): void {
    const uri = fileUri.getUriFromPath(filePath);
    const link = 'filemanager://openDirectory';
    const openLinkOptions: OpenLinkOptions = {
      appLinkingOnly: false,
      parameters: {
        'fileUri': uri
      }
    };
    try {
      this.ctx.openLink(link, openLinkOptions);
      hilog.info(DOMAIN, TAG, 'openItemInFolder OK: %{public}s', filePath);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'openItemInFolder failed: %{public}s', JSON.stringify(e));
    }
  }

  /**
   * Open a file with the system default handler.
   * Electron: FileManagerAdapter.openVerifiedItem
   */
  private openVerifiedItem(filePath: string): void {
    const uri = fileUri.getUriFromPath(filePath);
    const want: Want = {
      action: 'ohos.want.action.viewData',
      uri: uri,
      flags: wantConstant.Flags.FLAG_AUTH_WRITE_URI_PERMISSION |
        wantConstant.Flags.FLAG_AUTH_READ_URI_PERMISSION,
    };
    try {
      this.ctx.startAbility(want);
      hilog.info(DOMAIN, TAG, 'openVerifiedItem OK: %{public}s', filePath);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'openVerifiedItem failed: %{public}s', JSON.stringify(e));
    }
  }
}
