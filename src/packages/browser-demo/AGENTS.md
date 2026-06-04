# Lynxtron Application — Agent Guide

This document equips AI agents to author and modify code in this Lynxtron application quickly and safely. It explains the architecture, file layout, commands, and code patterns that work well with Lynx + NodeJS.

## Overview

- Lynxtron is an Electron-like runtime where `BrowserWindow` is replaced by `LynxWindow`.
- UI is built with Lynx via ReactLynx (`@lynx-js/react`) using lowercase built-in elements such as `<view>`, `<text>`, `<image>`.
- The NodeJS main process uses `lynxtron` APIs (a subset of Electron) to manage windows and load Lynx bundles.

## Project Layout

- `src/app`: Lynx UI layer built with ReactLynx
  - Entry: `src/app/index.tsx` renders `<App />` with `root.render`.
  - UI: `src/app/App.tsx` uses Lynx built-in elements and CSS.
  - Tests: `src/app/__tests__/index.test.tsx` (Vitest + @lynx-js/react/testing-library).
- `src/main`: NodeJS main process for Lynxtron
  - `src/main/main.ts` creates a `LynxWindow`, listens for events, and loads the Lynx bundle.
  - `src/main/vendorPaths.ts` defines paths to packaged assets (e.g., `main.lynx.bundle`).
- Config: `lynx.config.ts` sets RSpeedy bundler options for the Lynx UI.
- Builder: `rspack.config.ts` builds main process code and copies bundled UI artifacts.

Project integration reminders:
- UI entry: `src/app/index.tsx` with `root.render(<App />)`.
- Main process: `src/main/main.ts` uses `LynxWindow` and `loadFile`.
- Bundling: `lynx.config.ts` and `rspack.config.ts` coordinate output paths.

## Commands

Use NodeJS ≥ 22 and TypeScript.

- Install: `npm install`
- Build: `npm run build`
- Start app: `npm run start`
- Dev: `npm run dev`
- Test: `npm run test`

## Authoring UI (ReactLynx)


### Read in Advance


Read the docs below in advance to help you understand the library or frameworks this project depends on.

- Lynx: [llms.txt](https://lynxjs.org/llms.txt).
  While dealing with a Lynx task, an agent **MUST** read this doc because it is an entry point of all available docs about Lynx.

### Other UI API Key Differences from the Web

- Event model: `onclick` is not supported. Use Lynx mouse and touch events.
  - Mouse: `mousedown`, `mousemove`, `mouseup`, `mouseenter`, `mouseover`, `mouseleave`. See https://lynxjs.org/api/lynx-api/event/mouse-event.html
  - Touch: `touchstart`, `touchmove`, `touchend`, `touchcancel`, `tap`, `longpress`, `click`. See https://lynxjs.org/api/lynx-api/event/touch-event.html. 
- Not a browser: Lynx is web-like but not the Web. Do not rely on Web compatibility features such as `-webkit-` CSS prefixes, browser-specific behaviors, or deprecated Web APIs. Use only Lynx-supported styles and component capabilities.
- No Web DOM APIs: Avoid `document`/`window`/`HTMLElement` patterns. Interact via Lynx component refs and official APIs.

### Using Lynx Docs MCP

@lynx-js/docs-mcp-server lets your coding agent (such as Gemini, Claude, Cursor or Copilot) access Lynx documentation to assist you in development tasks. Therefore, we have specifically optimized llms.txt, a condensed version of the documentation site optimized for LLMs.

Configs for Lynx Docs MCP
```
{
  "mcpServers": {
    "lynx-docs": {
      "command": "npx",
      "args": ["-y", "@lynx-js/docs-mcp-server@latest"]
    }
  }
}
```
For any questions or requirements regarding Lynx through Lynx Docs MCP:

1. Use the "List Resources Tool" to list all Resources provided in MCP "lynx-docs".
2. First read MCP Resources "lynx-docs://llms.txt" (**REQUIRED**), this document is an ENTRYPOINT of all Lynx Docs.
3. After reading "lynx-docs://llms.txt", use the "Read MCP Resources Tool" to retrieve docs you need based on the user's questions or requirements, please read them proactively.
4. If available, prioritize obtaining Lynx-related information through MCP Resources tools over external web searches.

## Bundling (RSpeedy)

The Lynx UI is built with RSpeedy and configured in `lynx.config.ts`.

- `defineConfig` wraps the config for typing.
- `source.entry.main` points to `src/app/index.tsx`.
- `output.assetPrefix` uses a `file://` scheme to load assets from packaged `dist/assets`.
- Plugins include ReactLynx integration, type checking, dev-ready marker, and QR code.


## Testing

- Run tests: `npm run test`
- Framework: Vitest + `@lynx-js/react/testing-library`
- Snapshot and query helpers access the Lynx element tree.

## Conventions

- Use `@lynx-js/react` hooks and `root.render`.
- Prefer built-in elements and CSS modules or inline styles.
- Keep imports consistent with existing files and avoid adding new libraries unless necessary.
- Use the `@assets` alias for images and static resources.
- Do not use Web DOM APIs; rely on Lynx APIs and component refs.

## Common Recipes

Add a new UI component:

1. Create `src/app/MyPanel.tsx` exporting a ReactLynx function component using built-in elements.
2. Import and render it from `src/app/App.tsx`.
3. Add styles to `src/app/App.css` or `MyPanel.css` and reference via `className`.

Send a global event from main to UI:

1. In `src/main/main.ts`, call `w.sendGlobalEvent('refresh', { ts: Date.now() })` after `w.show()`.
2. In the UI, subscribe via your chosen global event hook or data model and update component state.

Adjust asset paths:

- Update `lynx.config.ts` `output.assetPrefix` and `rspack.config.ts` copy patterns if you change output locations.



## Local Type Definitions (Source of Truth)

- After `npm install`, always prefer local type definitions in `node_modules` for the exact API surface of your installed version.
- Package types for Lynxtron are exposed via the package `types` field and files under `apis`.

Where to inspect:
- `node_modules/@lynx-js/lynxtron/apis/lynxtron.d.ts` (entry types)
- `node_modules/@lynx-js/lynxtron/apis/api/lynx-window.d.ts` (window options, `loadFile`, `updateData`, `sendGlobalEvent`)
- `node_modules/@lynx-js/lynxtron/apis/api/app.d.ts` (app lifecycle)
- `node_modules/@lynx-js/lynxtron/apis/api/shell.d.ts`, `menu.d.ts`, etc. (subset of Electron APIs)
