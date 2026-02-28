# Lynxtron Application — Agent Guide

This document equips AI agents to author and modify code in this Lynxtron application quickly and safely. It explains the architecture, file layout, commands, and code patterns that work well with Lynx + NodeJS.

## Learning Resources

Read the docs below in advance to help you understand the library or frameworks this project depends on.

- Lynx: [llms.txt](https://lynxjs.org/llms.txt).
  While dealing with a Lynx task, an agent **MUST** read this doc because it is an entry point of all available docs about Lynx.

## Overview

- Lynxtron is an Electron-like runtime where `BrowserWindow` is replaced by `LynxWindow`.
/* WEB_SUPPORT_START */
- This project also supports Web (browser) via a Symmetric Host paradigm.
/* WEB_SUPPORT_END */
- UI is built with Lynx via ReactLynx (`@lynx-js/react`) using lowercase built-in elements such as `<view>`, `<text>`, `<image>`.
- **Symmetric Host Model**: Both Desktop and Web provide a consistent set of Native Modules to the UI.
  - `NativeModules.bridge`: Handles host-specific capabilities (dialogs, window control) via RPC.
  - `NativeModules.nodejs`: Provides a Node.js-like environment for background logic, injected directly into the Lynx Background Thread for maximum performance and object-level access.

## Project Layout

- `src/app`: Lynx UI layer built with ReactLynx
  - Entry: `src/app/index.tsx` renders `<App />` with `root.render`.
  - UI: `src/app/App.tsx` uses Lynx built-in elements and CSS.
- `src/main`: Host process logic
  - `src/main/desktop/`: Desktop (Node.js) host implementation.
    - `main.ts`: Main process entry (window management).
    - `preload.ts`: Logic injected into the Lynx thread (Node.js environment).
  /* WEB_SUPPORT_START */
  - `src/main/web/`: Web (Browser) host implementation.
    - `web-host.ts`: Browser entry point using `@lynx-js/lynxtron/web-host`.
    - `nodejs_adapter_web.ts`: Web adapter simulating Node.js capabilities in the Lynx Background Worker.
  /* WEB_SUPPORT_END */
- Config: `lynx.config.ts` (RSpeedy) and `rspack.config.ts` (Host builder).

## Common Patterns

### Calling Native Capabilities
Always use the unified `NativeModules` API to ensure cross-platform compatibility.

```typescript
// Unified call for both Desktop and Web
NativeModules.bridge.request({ method: 'showDialog', params: { message: 'Hi' } });

// Background logic (runs in the same JS thread as Lynx logic)
// Use getExposed() to access capabilities exported by host preload scripts
NativeModules.nodejs.getExposed().echo('Hello', (res) => {
  console.log(res);
});
```

## Commands

Use NodeJS ≥ 22 and TypeScript.

- Install: `npm install`
- Dev (Desktop): `npm run dev`
- Dev (Web): `npm run dev:web`
- Build All: `npm run build`
- Start (Desktop): `npm start`
- Start (Web): `npm run start:web`
- Test: `npm run test`

## Authoring UI (ReactLynx)

UI code in `src/app` runs in the Lynx engine, which is **not a browser**.

- **React-like but different**: Use `@lynx-js/react`.
- **Built-in Elements**: Use **lowercase** Lynx elements. DO NOT use HTML elements like `div`, `span`, `button`.
  - `<view>`: Container (like `div`).
  - `<text>`: Text (like `span`).
  - `<image>`: Image (like `img`).
  - `<scroll-view>`: Scrollable area.
  - `<list>`: High-performance list.
- **Event Model**: Standard Web events like `onClick` or `onChange` are NOT supported.
  - Use `bindtap` instead of `onClick`.
  - Use `bindinput` instead of `onChange`.
  - Events follow the pattern `bind<event_name>`.
- **CSS / Styling**:
  - Lynx uses a subset of CSS.
  - **Flexbox** is the primary layout engine (similar to React Native).
  - Use `className` for styling.
  - No CSS selectors like `:hover`, `nth-child`, or complex combinators.
- **No DOM/BOM APIs**: `window`, `document`, `location`, `localStorage` are NOT available.
  - Use `NativeModules.bridge` for host interactions.
  - Use `NativeModules.nodejs` for background logic and data persistence.
- **Main vs Background**: 
  - `src/app` runs in the Lynx Background thread (Main Thread in Lynx terminology).
  - It has direct access to `NativeModules.nodejs`.

### UI Example

```tsx
import { useState, useCallback } from '@lynx-js/react';

export function MyComponent() {
  const [count, setCount] = useState(0);

  const handleTap = useCallback(() => {
    setCount(c => c + 1);
  }, []);

  return (
    <view className="container">
      <text className="title">Count: {count}</text>
      <view className="button" bindtap={handleTap}>
        <text className="button-text">Increment</text>
      </view>
    </view>
  );
}
```

## Local Type Definitions

Inspect local types for exact API surfaces:
- `node_modules/@lynx-js/lynxtron/apis/lynxtron.d.ts`
- `node_modules/@lynx-js/lynxtron/apis/web-host.d.ts`
