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

## Files (excluding `apis/`)
- `package.json` — ESM config (`type: "module"`), `bin` entry, `types` entry, `postinstall`, dependencies, publish `files`.
- `install.js` — Postinstall script to download and extract the runtime to `dist/<platform>/<arch>/`; sets executable permission on macOS.
- `lynxtron_bin.js` — Resolves the platform-specific executable path under `dist/<platform>/<arch>/<exe>`.
- `utils/env-config.js` — Resolves platform, arch and version; builds the executable filename for each OS.
- `utils/download.js` — Download helper using `node-fetch` with timeout and single-write to disk.
- `scripts/scan-cjk-comments.js` — Dev utility to scan Chinese comments outside `front` (not published).

## Type Definitions
- Types entry: `./apis/lynxtron.d.ts`
