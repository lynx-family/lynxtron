import common from '@ohos.app.ability.common';
import window from '@ohos.window';
import { LynxWindowProxy } from './LynxWindowProxy';

export class AbilityManager {
  private static readonly proxies: Map<number, LynxWindowProxy> = new Map();
  // Map from C++-allocated window id to the same proxy. Used when a child
  // window is created before its parent's HarmonyOS window id has been bound.
  private static readonly cppToProxy: Map<number, LynxWindowProxy> = new Map();

  // Registers a proxy under both its HarmonyOS origin window id and, when
  // already known, its C++ window id.
  static addProxy(proxy: LynxWindowProxy): void {
    AbilityManager.proxies.set(proxy.getOriginWindowId(), proxy);
    const cppId = proxy.getCppWindowId();
    if (cppId > 0) {
      AbilityManager.cppToProxy.set(cppId, proxy);
    }
  }

  // Removes the proxy from both lookup maps.
  static removeProxy(proxy: LynxWindowProxy): void {
    AbilityManager.cppToProxy.delete(proxy.getCppWindowId());
    AbilityManager.proxies.delete(proxy.getOriginWindowId());
  }

  // Re-binds a proxy to a new C++ window id (first-window binding).
  static updateCppWindowId(proxy: LynxWindowProxy, newCppWindowId: number): void {
    AbilityManager.cppToProxy.delete(proxy.getCppWindowId());
    proxy.setCppWindowId(newCppWindowId);
    AbilityManager.cppToProxy.set(newCppWindowId, proxy);
  }

  // Looks up a proxy by HarmonyOS origin window id.
  static get(windowId: number): LynxWindowProxy | undefined {
    return AbilityManager.proxies.get(windowId);
  }

  // Looks up a proxy by C++ window id.
  static getByCppWindowId(cppWindowId: number): LynxWindowProxy | undefined {
    return AbilityManager.cppToProxy.get(cppWindowId);
  }

  // Looks up a proxy by HarmonyOS origin window id (explicit variant of get()).
  static getByOriginWindowId(originWindowId: number): LynxWindowProxy | undefined {
    return AbilityManager.proxies.get(originWindowId);
  }

  // Finds the proxy whose Ability context is the given object.
  static getByContext(context: common.UIAbilityContext): LynxWindowProxy | undefined {
    for (const proxy of AbilityManager.proxies.values()) {
      if (proxy.getWindowContext() === context) {
        return proxy;
      }
    }
    return undefined;
  }

  // Returns the placeholder proxy created by EntryAbility before its C++ window
  // id is known (cppWindowId === 0).
  static getMainProxy(): LynxWindowProxy | undefined {
    for (const proxy of AbilityManager.proxies.values()) {
      if (proxy.getCppWindowId() === 0) {
        return proxy;
      }
    }
    return undefined;
  }

  // Returns the origin window ids of all currently registered proxies.
  static getAllWindowIds(): number[] {
    return Array.from(AbilityManager.proxies.keys());
  }

  // Backward-compatible helpers used by adapters.
  static getContext(windowId: number): common.UIAbilityContext | undefined {
    return AbilityManager.proxies.get(windowId)?.getWindowContext();
  }

  static getWindowStage(windowId: number): window.WindowStage | undefined {
    return AbilityManager.proxies.get(windowId)?.getWindowStage();
  }

  static getOriginWindowId(windowId: number): number {
    return AbilityManager.proxies.get(windowId)?.getOriginWindowId() ?? -1;
  }
}
