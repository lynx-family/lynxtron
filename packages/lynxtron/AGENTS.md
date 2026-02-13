# Lynxtron Framework Package — Agent Guide

This document is intended to help AI Agents understand the structure, design philosophy, and usage of the `@lynx-js/lynxtron` package. Lynxtron is a cross-platform runtime framework that supports running Lynx applications on Desktop (based on Electron stack) and Web (based on Browser technology).

## Core Architecture and Environments

Lynxtron uses a **Symmetric Host** architecture to ensure that business logic code (running in Worker/Background threads) has a consistent development experience across different platforms.

### 1. Runtime Environment Classification

| Environment ID | Description | Desktop (Electron) | Web (Browser) | Recommended Import |
| :--- | :--- | :--- | :--- | :--- |
| **Main** | Main thread/process, responsible for window management, native capability bridging | Node.js (Electron Main) | Browser UI Thread | `import ... from '@lynx-js/lynxtron'` (Desktop)<br>`import ... from '@lynx-js/lynxtron/web-host'` (Web) |
| **Worker** | Business logic/background thread, responsible for running Lynx applications and adapter scripts | Node.js (Preload/Renderer) | Web Worker | `import ... from '@lynx-js/lynxtron/context-bridge'` |

### 2. Export Modules Breakdown

#### A. Main Entry (`@lynx-js/lynxtron`)
- **Applicable Environment**: Desktop Main Process
- **Content**: Exports full Lynxtron/Electron APIs (such as `app`, `LynxWindow`, `dialog`, etc.).
- **Note**: This module contains Node.js dependencies and **MUST NOT** be imported in Web or Web Worker environments.

#### B. Web Host (`@lynx-js/lynxtron/web-host`)
- **Applicable Environment**: Web Browser Main Thread
- **Content**: Provides APIs like `setupSymmetricHost` to initialize the Lynxtron simulation environment in the browser DOM.
- **Functionality**: Responsible for injecting `contextBridge` into the Worker and handling RPC calls from the Worker.

#### C. Context Bridge (`@lynx-js/lynxtron/context-bridge`)
- **Applicable Environment**: **Cross-Platform Universal** (Desktop Preload / Web Adapter Worker)
- **Content**: Provides the `contextBridge` object for injecting Host capabilities into the Lynx runtime (Lynx Background Thread Service).
- **Implementation Principle**:
  - **Desktop**: References native C++ modules or Electron `contextBridge`.
  - **Web**: Uses pure JS Polyfill to communicate with Web Host.
  - **Automatic Distribution**: The `exports` field in `package.json` automatically points to different implementations based on the runtime environment (`node` vs `browser`), ensuring code portability.

## Development Best Practices

### 1. Writing Cross-Platform Adapter/Preload Scripts
When writing adapter code that runs in the Lynx background thread (e.g., `preload.ts` or `nodejs_adapter.ts`):
- **MUST** use `import { contextBridge } from '@lynx-js/lynxtron/context-bridge';`
- **MUST NOT** use `import { contextBridge } from '@lynx-js/lynxtron';` (this will cause Web build failures).

**Incorrect Example**:
```typescript
// ❌ Error: Importing Node.js dependencies in Web environment will cause errors
import { contextBridge } from '@lynx-js/lynxtron';
```

**Correct Example**:
```typescript
// ✅ Correct: Automatically adapts to Desktop and Web
import { contextBridge } from '@lynx-js/lynxtron/context-bridge';

contextBridge.exposeInLynxBTS({
  // ...
});
```

### 2. Type Definitions
- The framework has built-in TypeScript definitions.
- The type definition for `contextBridge` is located in `apis/api/context-bridge.d.ts` and is exported in the package.

## Maintenance Guide

- **Adding New APIs**:
  - If it is a Main Process API, add it to `lynxtron.js` and `apis/`.
  - If it is a cross-platform utility, consider its dependencies and export it via a subpath to isolate environments if necessary.
- **Build Mechanism**: This package mainly distributes JS source code and binary download scripts, without complex build steps. The Web-related Polyfill (`web-worker.js`) is a pure JS implementation.
