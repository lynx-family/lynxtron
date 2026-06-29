# @lynx-js/cef-webview

A x-webview element implementation based on Chromium Embedded Framework (CEF) for Lynxtron.

## Overview

This library provides a CEF-based implementation of the x-webview element for Lynxtron applications. It allows you to embed Chromium-based web content within your Lynxtron app, providing a full-featured web browsing experience.

## Installation

```bash
npm install @lynx-js/cef-webview
```

## Usage

Enable the Lynxtron autolink plugin in your application build. AutoLink requires
`@lynx-js/cef-webview/lynxtron`, which loads the current platform's Lynxtron
addon so its static Lynx registrations run during startup. CEF itself is
initialized only when you call `initialize()`.

```ts
import cefWebview from '@lynx-js/cef-webview/lynxtron';

cefWebview.initialize();
```

Once initialized, you can use the x-webview element in your Lynx templates:

```xml
<x-webview src="https://www.example.com" width="100%" height="500px"></x-webview>
```

## Building

### Prerequisites

- Node.js >= 18
- CMake
- CEF SDK

### Build Steps

1. Clone the repository
2. Install dependencies:
   ```bash
   npm install
   ```
3. Build the native addon:
   ```bash
   npx cmake-js build
   ```

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
