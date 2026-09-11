# @lynx-js/cef-webview

A `<webview>` element implementation based on Chromium Embedded Framework (CEF) for Lynxtron.

## Overview

This library provides a CEF-based implementation of the `<webview>` element for Lynxtron applications. It allows you to embed Chromium-based web content within your Lynxtron app, providing a full-featured web browsing experience.

## Installation

```bash
npm install @lynx-js/cef-webview
```

## Usage

Enable the Lynxtron autolink plugin in your application build. AutoLink requires
`@lynx-js/cef-webview/lynxtron`, which loads the current platform's Lynxtron
addon so its static Lynx registrations run during startup. CEF itself is
initialized only when you call `initialize()`.

The package registers `<webview>` through the native AutoLink registration API
and selects its addon from the literal target paths in `lynx.lib.json`.
The manifest also declares the CEF Framework/helper bundles on macOS and the
DLLs, subprocess and resource files on Windows for development staging.

When packaging the app, lynxtron-builder places the declared macOS Framework
and helper app bundles in `Contents/Frameworks` before signing. Windows native
runtime files remain adjacent to the addon outside ASAR.

```ts
import cefWebview from '@lynx-js/cef-webview/lynxtron';

cefWebview.initialize();
```

`initialize()` throws if native CEF initialization returns `false`. Do not
continue creating WebViews after this error. The current implementation shares
the default CEF storage directory across host applications, so only one host
process can initialize the CEF browser at a time. That process can create multiple
WebViews; CEF helper subprocesses are not additional browser host processes.
Close other applications running WebView and retry. This is not the only possible
cause of initialization failure; check the native logs for the actual cause.

Once initialized, you can use the `<webview>` element in your ReactLynx components:

```tsx
<webview
  src="https://www.example.com"
  style={{ width: '100%', height: '500px' }}
/>
```

## Building

### Prerequisites

- Node.js matching this package's engines
- CMake
- CEF SDK

### Build Steps

After preparing the repository's native dependencies, run the package build
from `src`:

```bash
node tools/yarn.js workspace @lynx-js/cef-webview build
```

This builds the addon and stages runtime files in `dist/<platform>/<arch>`.
On macOS it also preserves the CEF Framework links; the install script restores
those links when npm packaging omits them.

On Windows, build the Lynxtron source runtime first, then run the same entry used
by CI and release builds from the repository root:

```powershell
.\lynxtron_tools\build_cef_webview.ps1 -Arch x64
```

The script syncs CEF dependencies and passes `out/Release/lynxtron.dll.lib`
explicitly to the package build. Use `-ImportLibrary <path>` for another
source-build output. A missing import library fails before compilation instead
of falling back to a downloaded runtime.

### macOS cross-compilation

From the repository root, use the same entry point as the release workflow:

```bash
python3 lynxtron_tools/build_cef_webview.py --arch x64
```

The script prepares host tools and the target CEF SDK, builds the package and
checks the addon, Framework and helper architectures. Use `--arch arm64` for
Apple Silicon or add `--version <version>` to produce a release zip in
`publish/`. No Rosetta Node process or manual Homebrew installation is required.

## Dependencies

- **Runtime Dependencies:**
  - js-yaml
  - plist

- **Development Dependencies:**
  - node-addon-api
  - cmake-js
  - lynxtron

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Authors

Lynxtron Authors
