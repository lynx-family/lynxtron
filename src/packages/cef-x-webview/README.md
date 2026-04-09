# @lynx-js/cef-x-webview

A x-webview element implementation based on Chromium Embedded Framework (CEF) for Lynxtron.

## Overview

This plugin provides a CEF-based implementation of the x-webview element for Lynxtron applications. It allows you to embed Chromium-based web content within your Lynxtron app, providing a full-featured web browsing experience.

## Installation

```bash
npm install @lynx-js/cef-x-webview
```

## Usage

To use this plugin in your Lynxtron application, you need to initialize it before creating any web views:

```javascript
const cefPlugin = require('@lynx-js/cef-x-webview');
cefPlugin.setUp();
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
3. Build the native extension:
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
