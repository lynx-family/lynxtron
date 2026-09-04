---
'@lynx-js/lynx-library-headers': patch
'@lynx-js/cef-webview': patch
'@lynx-js/lynxtron': patch
'@lynx-js/lynxtron-builder': patch
'@lynx-js/lynxtron-dev-plugins': patch
'@lynx-js/lynxtron-rebuild': patch
'create-lynxtron': patch
---

Declare support for Node.js 24 and 26 across all published Lynxtron packages
while retaining Node.js 22.18 and later within the Node.js 22 release line.
Use the maintained zip-lib extraction APIs, backed by yauzl 3, so binary
installation completes safely on Node.js 26. Serialize concurrent runtime
installation across processes so simultaneous first launches reuse one download.

Publish Release and DevTool runtimes under one package version. The `lynxtron`
CLI defaults to DevTool while `lynxtron-builder` defaults to Release, with CLI,
environment, and electron-builder.yml overrides for explicit selection.
Download missing runtimes from the matching Lynxtron GitHub Release by default,
including the DevTool runtime installed by npm postinstall.
Allow source builds to skip that postinstall download while their matching
runtime release does not exist yet.
Load the generated application's Lynx bundle directly from its ASAR and stop
copying the complete desktop output as an extra resource. Package AutoLink
libraries only from `.lynxtron/native`, without a duplicate regular
`node_modules` payload.
Keep Release and DevTool downloads under distinct electron-builder cache keys so
switching variants at the same version cannot reuse the other runtime archive.
Publish both macOS x64 and arm64 runtime archives so universal packaging can
resolve both slices for Release and DevTool builds.
