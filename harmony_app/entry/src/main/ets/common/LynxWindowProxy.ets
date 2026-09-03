import common from '@ohos.app.ability.common';
import window from '@ohos.window';

// Unified proxy interface for AbilityManager. A BaseWindowAbility instance
// implements this interface so that window management logic can operate on
// windows without knowing the concrete Ability class.
export interface LynxWindowProxy {
  // The OS window object for this proxy.
  getWindow(): window.Window;
  // The window stage; for subwindows this is the parent Ability's stage.
  getWindowStage(): window.WindowStage;
  // The Ability context backing this window.
  getWindowContext(): common.UIAbilityContext;
  // HarmonyOS origin window id.
  getWindowId(): number;
  // HarmonyOS origin window id (same as getWindowId()).
  getOriginWindowId(): number;
  // The C++-allocated window id this proxy is bound to (0 = unbound).
  getCppWindowId(): number;
  // Re-binds the proxy to a different C++ window id.
  setCppWindowId(id: number): void;
  // Whether this proxy represents the first (entry) main Ability window.
  isMainAbility(): boolean;
}
