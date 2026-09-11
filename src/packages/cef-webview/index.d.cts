/** CEF-backed WebViews for Lynxtron applications. */
declare namespace cefWebview {
  /**
   * Initializes CEF before creating WebViews.
   *
   * The current implementation shares the default CEF storage directory across
   * host applications. Only one host process can initialize the CEF browser at
   * a time. That process can create multiple WebViews; CEF helper subprocesses
   * are not additional browser host processes.
   *
   * @returns `true` when native initialization succeeds.
   * @throws If native initialization fails. Do not continue creating WebViews.
   * Close other applications running WebView and retry. Initialization can also
   * fail for other reasons; check the native logs for the actual cause.
   */
  function initialize(): boolean;
}

export = cefWebview;
