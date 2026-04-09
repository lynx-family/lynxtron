# Lynxtron Shell Demo

## Tech Stack

- `lynx`
- `lynxtron`
- `TypeScript`
- `React`
- `Rspack` + `Electron-Builder` (JS minification and application packaging)

## Features

- One-click run, debug, and package
- Supports TypeScript for type safety
- A complete engineering experience
- Supports packaging for various platforms using `electron-builder`

## Prerequisites

- NodeJS >= 22
- TypeScript
- [LynxDevTool](https://github.com/lynx-family/lynx-devtool/releases/) >= 0.1.1

## Usage Guide

### Install Dependencies

```bash
npm install
```

### One-Click Start

Run the following command to compile the code and start the application with a single command.

```bash
npm run start
```

### Debugging

This project supports debugging Lynx (renderer process) and Lynxtron (main process) separately or simultaneously.

- **Debug Lynx and Lynxtron Simultaneously (Recommended)**

  Run the following command in a terminal to start both Lynx and Lynxtron in debug mode with hot-reloading enabled.

  ```bash
  npm run dev
  ```

- **Debug Lynx Only**

  If you only need to focus on the UI, you can start the Lynx debug server alone. You will need to open the **LynxDevTool** to debug.

  ```bash
  npm run dev:lynx
  ```

- **Debug Lynxtron Only**

  If you only need to focus on the main process logic, you can start the Lynxtron debug server alone.
  **Note**: This command will wait for the Lynx development server (`http://localhost:3000`) to be ready before executing.

  ```bash
  npm run dev:node
  ```
  The debug port for the Lynxtron main process is `9222`. You need to open `chrome://inspect` in a Chrome browser and add a listener for port `9222` to debug.

### Application Packaging

Use the following commands to package the application for different platforms.

- **Package for macOS (x64)**

  ```bash
  npm run pack:mac:x64
  ```

- **Package for macOS (arm64)**

  ```bash
  npm run pack:mac:arm64
  ```

- **Package for macOS (Universal)**

  This command builds a universal application for macOS that runs on both x64 and arm64 architectures.

  ```bash
  npm run pack:mac:universal
  ```

- **Package for Windows (ia32)**

  ```bash
  npm run pack:win
  ```
