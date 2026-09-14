---
'@lynx-js/cef-webview': patch
'@lynx-js/lynxtron': patch
---

Update Lynx to include CEF application profile isolation on Windows and macOS and the macOS WebView message bridge fix. Windows uses the host AppUserModelID, with an executable-name fallback. Document identity setup and report profile conflicts without incorrectly rejecting concurrent applications with distinct identities.
