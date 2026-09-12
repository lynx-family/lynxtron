// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// =============================================================================
// -----------------------------------------------------------------------------
// 作用:
//   1. saveUris():  选文件持久化成功后, 把 URI 清单存进 preferences(本地)。
//   2. initPermissions(): 应用启动时, 读取之前存的 URI 清单,
//                        对每个 URI 调 fileShare.activatePermission() 重新激活,
//                        使"重启后仍可访问"生效(官方: Persist 存档 + Activate 激活)。
// 对应 Electron: ohos_hap .../adapter/PermissionManagerAdapter.ets
//   (saveUris / getUris / initPermissions / activateFileAccessPersist)
// =============================================================================
import common from '@ohos.app.ability.common';
import { fileShare } from '@kit.CoreFileKit';
import { BusinessError } from '@ohos.base';
import hilog from '@ohos.hilog';
import preferences from '@ohos.data.preferences';

const DOMAIN = 0x0000;
const TAG = 'PersistManager';

const PREF_NAME = 'lynxtron_persist_store';
const PREF_KEY = 'persisted_uris';

/**
 * 持久化文件访问管理(单例)。
 */
export class PersistManager {
  private static instance: PersistManager | null = null;
  private ctx: common.UIAbilityContext;

  /** 获取单例(需先 register)。 */
  static getInstance(): PersistManager {
    if (!PersistManager.instance) {
      throw new Error('PersistManager not registered yet');
    }
    return PersistManager.instance;
  }

  private constructor(ctx: common.UIAbilityContext) {
    this.ctx = ctx;
  }

  /**
   * 注册(由 BridgeRegistry.registerAll 调用)。注册时即做"启动激活"。
   */
  static register(context: common.UIAbilityContext): void {
    if (PersistManager.instance) {
      hilog.info(DOMAIN, TAG, 'Already registered, skipping');
      return;
    }
    PersistManager.instance = new PersistManager(context);
    // 启动时激活历史持久化的 URI (重启后权限恢复的关键一步)
    PersistManager.instance.initPermissions();
  }

  /**
   * 启动激活: 读之前存的 URI 清单, 逐个 activatePermission。
   * 官方: 重启后无需再 Persist, 直接 Activate 即可恢复访问。
   */
  initPermissions(): void {
    const uris = this.getUris();
    hilog.info(DOMAIN, TAG, '[PERSIST] initPermissions: uris=%{public}s',
      JSON.stringify(uris));
    if (uris.length === 0) {
      hilog.info(DOMAIN, TAG, '[PERSIST] no persisted uris to activate');
      return;
    }
    this.activateFileAccessPersist(uris);
  }

  /**
   * 把 URI 清单存进 preferences(供重启后激活用)。
   */
  saveUris(uris: string[]): void {
    if (uris.length === 0) {
      return;
    }
    try {
      const pref = preferences.getPreferencesSync(this.ctx, { name: PREF_NAME });
      pref.putSync(PREF_KEY, JSON.stringify(uris));
      pref.flush((err: BusinessError) => {
        if (err) {
          hilog.error(DOMAIN, TAG, '[PERSIST] saveUris flush failed: %{public}s',
            JSON.stringify(err));
        } else {
          hilog.info(DOMAIN, TAG, '[PERSIST] saveUris OK: %{public}s',
            JSON.stringify(uris));
        }
      });
    } catch (e) {
      hilog.error(DOMAIN, TAG, '[PERSIST] saveUris error: %{public}s', String(e));
    }
  }

  /**
   * 读回之前存的 URI 清单。
   */
  private getUris(): string[] {
    try {
      const pref = preferences.getPreferencesSync(this.ctx, { name: PREF_NAME });
      const raw: string = pref.getSync(PREF_KEY, '[]') as string;
      const arr: object = JSON.parse(raw);
      return Array.isArray(arr) ? (arr as string[]) : [];
    } catch (e) {
      hilog.error(DOMAIN, TAG, '[PERSIST] getUris error: %{public}s', String(e));
      return [];
    }
  }

  /**
   * 对一批 URI 调 fileShare.activatePermission(激活生效)。
   */
  private activateFileAccessPersist(uris: string[]): void {
    const policies: fileShare.PolicyInfo[] = [];
    for (const uri of uris) {
      const policy: fileShare.PolicyInfo = {
        uri: uri,
        operationMode: fileShare.OperationMode.READ_MODE |
          fileShare.OperationMode.WRITE_MODE,
      };
      policies.push(policy);
    }
    hilog.info(DOMAIN, TAG, '[PERSIST] activatePermission begin, uris=%{public}d',
      uris.length);
    fileShare.activatePermission(policies).then(() => {
      hilog.info(DOMAIN, TAG, '[PERSIST] activatePermission OK, uris=%{public}d',
        uris.length);
    }).catch((err: BusinessError<Array<fileShare.PolicyErrorResult>>) => {
      hilog.error(DOMAIN, TAG, '[PERSIST] activatePermission FAILED: %{public}s',
        JSON.stringify(err));
    });
  }
}