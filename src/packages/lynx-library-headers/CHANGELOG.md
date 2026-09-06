# @lynx-js/lynx-library-headers

## 0.0.20

### Patch Changes

- 331ae3f: Fix CEF webview publish build by using yarn instead of npm to resolve workspace:\* protocol.
- Updated dependencies [331ae3f]
  - @lynx-js/lynxtron@0.0.20

## 0.0.19

### Patch Changes

- d23dd56: Declare support for Node.js 24 and 26 across all published Lynxtron packages
  while retaining Node.js 22.18 and later within the Node.js 22 release line.
  Use the maintained zip-lib extraction APIs, backed by yauzl 3, so binary
  installation completes safely on Node.js 26. Serialize concurrent runtime
  installation across processes so simultaneous first launches reuse one download.

  Publish Release and DevTool runtimes under one package version. The `lynxtron`
  CLI defaults to DevTool while `lynxtron-builder` defaults to Release, with CLI,
  environment, and electron-builder.yml overrides for explicit selection.
  Keep Release and DevTool downloads under distinct electron-builder cache keys so
  switching variants at the same version cannot reuse the other runtime archive.
  Publish both macOS x64 and arm64 runtime archives so universal packaging can
  resolve both slices for Release and DevTool builds.

- Updated dependencies [d23dd56]
  - @lynx-js/lynxtron@0.0.19
