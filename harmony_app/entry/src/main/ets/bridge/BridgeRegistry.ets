import common from '@ohos.app.ability.common';
import hilog from '@ohos.hilog';
import { PlatformBridge } from './PlatformBridge';
import { FileDialogBridge } from './FileDialogBridge';
import { PowerMonitorBridge } from './PowerMonitorBridge';
import { PersistManager } from './PersistManager';

const DOMAIN = 0x0000;
const TAG = 'BridgeRegistry';

/**
 * Unified registration entry for all platform bridges.
 *
 * Each bridge lives in its own file with a static register(ctx) method;
 * add a new bridge by appending one entry to the registrars array (e.g.
 * AaaBridge.register).  EntryAbility only needs to call
 * BridgeRegistry.registerAll(context) once.
 */
export class BridgeRegistry {
  private static registrars: Array<(ctx: common.UIAbilityContext) => void> = [
    (ctx) => PlatformBridge.register(ctx),
    (ctx) => FileDialogBridge.register(ctx),
    (ctx) => PowerMonitorBridge.register(ctx),
    (ctx) => PersistManager.register(ctx)
  ];

  static registerAll(context: common.UIAbilityContext): void {
    for (const registrar of BridgeRegistry.registrars) {
      try {
        registrar(context);
        hilog.info(DOMAIN, TAG, 'bridge registered');
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'bridge register failed: %{public}s',
          JSON.stringify(e));
      }
    }
  }
}
