// TypeScript declaration for the lynxtron NAPI module.
// Matches the napi_property_descriptor exported from
// src/shell/app/lynxtron_napi_bridge.cc::Init.

declare namespace lynxtron {
  /**
   * Run LynxtronMain in the calling thread. Bring-up smoke test;
   * future versions will accept a config object and dispatch on a
   * worker thread so the UIAbility main thread isn't blocked.
   *
   * @returns LynxtronMain's exit code.
   */
  function start(): number;

  /**
   * Asks the browser to exit gracefully (Application::Quit), the same path as
   * JS app.quit(). Mirrors electron_ohos's kAppQuit command. After the graceful
   * quit completes, the bridge posts the exit code to the ArkUI thread through
   * a thread-safe function and calls terminateSelf() on the ability context —
   * no polling and no exit() (which appspawn would abort on).
   */
  function quit(): void;

  /** Sends one final UTF-8 IME/text commit to the focused Lynx editable. */
  function sendText(text: string): void;

  /** Cached IME visibility and view-relative caret rectangle from Lynx. */
  function getTextInputState(): {
    visible: boolean;
    x: number;
    y: number;
    width: number;
    height: number;
  };

  /**
   * Reconciles the system input method with Lynx's focus state. Call from the
   * ArkUI thread on a short interval: it attaches the native text editor proxy
   * when a Lynx editable takes focus, detaches it when focus is lost, and
   * otherwise keeps the candidate window anchored to the caret.
   *
   * @returns whether the input method is currently attached.
   */
  function syncIme(): boolean;

  /**
   * Publishes the id of the window hosting the XComponent surface, so the
   * input method service can route text to it. Call once from
   * onWindowStageCreate, before any editable can take focus.
   */
  function setWindowId(id: number): void;

  // ---- AppGallery Kit bridge (TSFN, no polling) ----
  // Registers a callback that receives request types from Node.js:
  //   type=0 → checkAppUpdate, type=1 → showUpdateDialog, type=2 → loadProduct
  function registerUpdateTSFN(callback: (type: number) => void): void;
  function consumeCheckAppUpdateRequest(): boolean;
  // Reports the JSON-serialized CheckUpdateResult back to C++.
  function resolveCheckAppUpdate(json: string): void;
  // Reports the ShowUpdateResultCode (int) back to C++.
  function resolveShowUpdateDialog(code: number): void;
  // Reports the loadProduct result (JSON) back to C++.
  function resolveLoadProduct(json: string): void;
}

export default lynxtron;
