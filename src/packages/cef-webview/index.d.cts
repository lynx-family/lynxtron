declare namespace cefWebview {
  interface InitializeOptions {
    /** Absolute root directory. Defaults to app.getPath('userData')/cef-webview. */
    storagePath?: string;
    /** Persist web data under storagePath/profile. Defaults to false. */
    persistent?: boolean;
  }
  /** Call after app.whenReady(). Throws on invalid settings or initialization failure. */
  function initialize(options?: InitializeOptions): boolean;
}
export = cefWebview;
