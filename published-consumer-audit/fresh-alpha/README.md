# Fresh Alpha

## Tech Stack

- `lynx`
- `lynxtron`
- `TypeScript`
- `React`
- `Rspeedy` + `Rsbuild` + `Electron-Builder` (bundling and application packaging)

## Features

- **Symmetric Host**: Identical UI code runs on both Desktop (Node.js) and Web (Browser).
- **Background Thread Injection**: `NativeModules.nodejs` provides high-performance background logic without blocking UI.
- One-click run, debug, and package.
- Supports TypeScript for type safety.

## Prerequisites

- Node.js 22 (>= 22.18.0), Node.js 24, or Node.js 26
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

- **Web (Browser)**
  ```bash
  npm run dev:web
  ```

### Build & Start

- **Build Desktop**

  ```bash
  npm run build
  ```

- **Start Desktop**
  ```bash
  npm start
  ```

- **Build Web**
  ```bash
  npm run build:web
  ```

- **Start Web**
  ```bash
  npm run start:web
  ```

### Application Packaging

- **Package Desktop Application**

  ```bash
  npm run pack
  ```
