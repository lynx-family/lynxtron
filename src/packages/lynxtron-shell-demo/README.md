# Lynxtron Shell Demo

## Tech Stack

- `lynx`
- `lynxtron`
- `TypeScript`
- `React`
- `Rspack` + `Electron-Builder` (JS minification and application packaging)

## Features

/* WEB_SUPPORT_START */
- **Symmetric Host**: Identical UI code runs on both Desktop (Node.js) and Web (Browser).
/* WEB_SUPPORT_END */
- **Background Thread Injection**: `NativeModules.nodejs` provides high-performance background logic without blocking UI.
- One-click run, debug, and package.
- Supports TypeScript for type safety.

## Prerequisites

- NodeJS >= 22
- [LynxDevTool](https://github.com/lynx-family/lynx-devtool/releases/) >= 0.1.1

## Usage Guide

### Install Dependencies

```bash
npm install
```

### Development

- **Desktop (Lynxtron)**
  ```bash
  npm run dev
  ```

/* WEB_SUPPORT_START */
- **Web (Browser)**
  ```bash
  npm run dev:web
  ```
/* WEB_SUPPORT_END */

### Build & Start

- **Build Desktop**
  ```bash
  npm run build
  ```

- **Start Desktop**
  ```bash
  npm start
  ```

/* WEB_SUPPORT_START */
- **Build Web**
  ```bash
  npm run build:web
  ```

- **Start Web**
  ```bash
  npm run start:web
  ```
/* WEB_SUPPORT_END */

### Application Packaging

- **Package for macOS (x64)**
  ```bash
  npm run pack:mac:x64
  ```

- **Package for macOS (arm64)**
  ```bash
  npm run pack:mac:arm64
  ```

- **Package for macOS (Universal)**
  ```bash
  npm run pack:mac:universal
  ```

- **Package for Windows (ia32)**
  ```bash
  npm run pack:win
  ```
