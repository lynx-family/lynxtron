# Lynxtron Front Package

A minimal npm package that distributes the Lynxtron runtime and exposes TypeScript type definitions with a simple CLI entry.

## Installation & Usage
- Requires Node.js `>=16`
- Install:

```bash
npm install lynxtron
# or
pnpm add lynxtron
```

- CLI:

```bash
npx lynxtron <args>
```

- Fuse CLI:

```bash
npx lynxtron-fuses read
npx lynxtron-fuses write runAsNode=off embeddedAsarIntegrityValidation=on onlyLoadAppFromAsar=on
npx lynxtron-fuses read --app "C:\\path\\to\\lynxtron"
npx lynxtron-fuses write --binary "C:\\path\\to\\lynxtron.exe" nodeOptions=off
```

When `--app` points at a packaged app, Lynxtron prefers the real runtime binary:
- macOS: `Contents/Frameworks/Lynxtron Framework.framework/Lynxtron Framework`
- Windows: `lynxtron.dll`

- Fuse API:

```js
import { flipFuses, FuseV1Options, FuseVersion, getCurrentFuses } from '@lynx-js/lynxtron/fuses';

await flipFuses('/Applications/Lynxtron.app', {
  version: FuseVersion.V1,
  [FuseV1Options.RunAsNode]: false,
  [FuseV1Options.EnableEmbeddedAsarIntegrityValidation]: true,
  [FuseV1Options.OnlyLoadAppFromAsar]: true,
});

console.log(await getCurrentFuses('/Applications/Lynxtron.app'));
console.log(await getCurrentFuses('C:\\path\\to\\lynxtron'));
```

## Files (excluding `apis/`)
- `package.json` — ESM config (`type: "module"`), `bin` entry, `types` entry, `postinstall`, dependencies, publish `files`.
- `fuses.js` — Reads and flips Lynxtron fuse bytes embedded in the packaged runtime.
- `fuses-cli.js` — CLI wrapper for reading and writing fuse values.
- `install.js` — Postinstall script to download and extract the runtime to `dist/<platform>/<arch>/`; sets executable permission on macOS.
- `lynxtron_bin.js` — Resolves the platform-specific executable path under `dist/<platform>/<arch>/<exe>`.
- `utils/env-config.js` — Resolves platform, arch and version; builds the executable filename for each OS.
- `utils/download.js` — Download helper using `node-fetch` with timeout and single-write to disk.
- `scripts/scan-cjk-comments.js` — Dev utility to scan Chinese comments outside `front` (not published).

## Type Definitions
- Types entry: `./apis/lynxtron.d.ts`

## Multi-Environment Support

Lynxtron supports both **Desktop** (Node.js/Electron) and **Web** (Browser) environments. To ensure compatibility, use the correct import paths:

- **Main Process (Desktop)**: `import { app, LynxWindow } from '@lynx-js/lynxtron'`
- **Web Host (Browser)**: `import { setupSymmetricHost } from '@lynx-js/lynxtron/web-host'`
- **Worker / Preload (Cross-Platform)**: `import { contextBridge } from '@lynx-js/lynxtron/context-bridge'`

### Context Bridge
When writing code that runs in the Lynx Background Thread (e.g., preload scripts or adapters), always import `contextBridge` from the subpath to ensure the correct implementation is loaded for the target environment (Native Module for Desktop, Polyfill for Web).

```typescript
import { contextBridge } from '@lynx-js/lynxtron/context-bridge';

contextBridge.exposeInLynxBTS({
  // ...
});
```
