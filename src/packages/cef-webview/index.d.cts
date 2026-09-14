/** CEF-backed WebViews for Lynxtron applications. */
declare namespace cefWebview {
  /**
   * Initializes CEF before creating WebViews.
   *
   * Root storage is isolated by the macOS host bundle ID or the Windows
   * AppUserModelID. On Windows, call app.setAppUserModelId() with a stable,
   * unique identity before initialization. Compatibility fallbacks (the
   * macOS Helper bundle ID or Windows executable name) may be shared by apps.
   * Different identities can run concurrently; hosts sharing a profile cannot.
   * A host can create multiple WebViews.
   *
   * @returns `true` when native initialization succeeds.
   * @throws If native initialization fails. Do not continue creating WebViews.
   * Check the native logs for the profile path and actual cause. If another
   * host uses the same profile, close it or choose a distinct app identity.
   */
  function initialize(): boolean;
}

export = cefWebview;
