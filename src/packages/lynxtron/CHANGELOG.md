# @lynx-js/lynxtron

## 0.0.26

### Patch Changes

- 787aff7: Let Windows choose the GPU in public runtime and CEF helper releases by default. Add a publish workflow option to request high-performance graphics while preserving the existing preference in source builds.

## 0.0.25

### Patch Changes

- c3b4877: Upgrade Lynx to give macOS surfaces unique texture lifetime IDs, preventing stale partial-repaint damage from leaving unpainted margins when Metal texture addresses are reused.
- 3c0dbe2: Start asynchronous Windows graphics prewarming before ICU and Node/V8 initialization to overlap D3D11 device preparation with application startup.

## 0.0.24

## 0.0.23

### Patch Changes

- 2a68cb1: Update Lynx to include CEF application profile isolation on Windows and macOS and the macOS WebView message bridge fix. Windows uses the host AppUserModelID, with an executable-name fallback. Document identity setup and report profile conflicts without incorrectly rejecting concurrent applications with distinct identities.

## 0.0.22

### Patch Changes

- e00824d: Build Windows CEF against the source-built Lynxtron import library and stage the
  addon, subprocess, and runtime resources through the package build command.
  Restore macOS Framework links omitted by npm packaging and adopt the upstream
  CEF fixes. Source builds can skip downloading an unpublished Lynxtron runtime.

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
