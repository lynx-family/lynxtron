# @lynx-js/cef-webview

## 0.0.22

### Patch Changes

- e00824d: Register and load the CEF webview through AutoLink. Select literal target-specific
  runtime artifacts from lynx.lib.json, stage them after emit, and resolve generated
  loaders from the application output without copying entire dependency packages.
- e00824d: Build Windows CEF against the source-built Lynxtron import library and stage the
  addon, subprocess, and runtime resources through the package build command.
  Restore macOS Framework links omitted by npm packaging and adopt the upstream
  CEF fixes. Source builds can skip downloading an unpublished Lynxtron runtime.
- e0fa832: Throw an actionable error when native CEF initialization fails and document the
  current single-browser-host-process limitation across applications.
- e00824d: Build macOS CEF artifacts for an explicit target architecture using host-native
  tools, Habitat-managed CMake and CEF SDKs, and the same entry point locally and
  in the release workflow. Verify native artifact architectures before packaging.

## 0.0.21

### Patch Changes

- 61aa47b: Fix skity rendering issue with paints_into_platform_view_slice limited to Window

## 0.0.20

### Patch Changes

- 331ae3f: Fix CEF webview publish build by using yarn instead of npm to resolve workspace:\* protocol.

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
