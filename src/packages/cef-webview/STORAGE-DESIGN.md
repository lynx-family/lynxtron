# Webview storage ownership

Lynxtron exposes generic app APIs, with no CEF dependency. Autolink loads native
registrations only. The webview adapter reads `app.getPath('userData')` lazily in
`initialize()` after readiness, then passes resolved UTF-8 paths through N-API to
the Lynx CEF initializer. The existing parameterless C API remains available.

`initialize({storagePath?, persistent?})` defaults to the absolute root
`<userData>/cef-webview`. Persistence defaults to false. A configured initializer
creates an explicit, plugin-shared RequestContext: empty cache path for in-memory
web data, or `<storagePath>/profile` for persistent web data. CEF Chrome bootstrap
backs its global context with a disk profile, even with an empty global cache
path. Therefore browser creation and all cookie APIs use the explicit context
together. Legacy C hosts without storage settings retain the global context.

Identical repeated JS initialization succeeds without reinitializing CEF.
Changing settings after initialization or native initialization failure throws.
On macOS, queued message-pump blocks retain their receiver and become no-ops
after initialization failure. The loaded CEF library remains resident until
process exit so pending reference-counted objects can be released safely.

Host userData defaults are application-name-based, not bundle-ID-based. Hosts
requiring stable ID isolation must configure `app.setPath('userData', ...)`
before initialization. Go must configure each showcase child process. This
change does not migrate existing host data or introduce per-webview partitions.
Installation-level CEF data may still exist under the root in memory mode; web
cookies and localStorage must not survive a process restart in that mode.

Profile isolation does not change CEF's macOS keychain naming or Windows
encryption policy. Do not disable encryption, migrate user keychain items, or
claim that native plugins are sandboxed by this directory selection. Runtime
user data is never packaged into npm archives or installers.

Real runtime regressions must cover memory and persistent restart, switching the
same root between modes without exposing or deleting persistent data, separate
root isolation, agreement between cookie APIs and pages, and duplicate-profile
startup failures. Fast-exit cookie durability is a separate deferred issue.
