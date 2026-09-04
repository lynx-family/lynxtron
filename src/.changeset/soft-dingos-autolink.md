---
'@lynx-js/cef-webview': patch
'@lynx-js/lynxtron-dev-plugins': patch
'@lynx-js/lynxtron-builder': patch
---

Declare the CEF binary and framework payloads in `lynx.lib.json`, and make
Lynxtron AutoLink consume only the prerelease target-based artifact schema.
Each `os`/`arch` target owns its `files`, optional `frameworks`, and macOS
`appBundles`. AutoLink stages only the selected target's declared artifacts.
Publish the Windows x64 CEF addon and runtime payload alongside the macOS
artifacts.

Package staged AutoLink libraries into final applications. macOS Frameworks
and nested app bundles are copied to `Contents/Frameworks`, while native
packages remain available from `app.asar.unpacked` on macOS and Windows.
Launch the development runtime with the application directory before optional
runtime flags so AutoLink always resolves the staged native package root.
