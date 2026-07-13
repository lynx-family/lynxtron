# Lynxtron Development Plugins

Rsbuild, Rspack, and Rspeedy integration for Lynxtron development flows.

## What It Does

- Integrates a Node-targeted Rsbuild environment with the Electron Main runtime.
- Runs Lynxtron AutoLink and stages native dependencies described by
  `lynx.lib.json`.
- Restarts the Lynxtron process after successful Desktop Host development
  compilations.
- Emits a one-time `RSBUILD_READY` line to stdout after the first successful dev compile.
- Prints the dev server port as `RSBUILD_DEV_SERVER:<port>` when it starts.
- Writes a `dev-ready.json` marker file with `{ "ready": true, "source": "rspeedy", "time": <epoch_ms> }`.
- Ships small CLIs that wait for marker files and exit successfully once ready.

## Why

In monorepos you often run multiple dev processes at once (e.g. RSBuild app server, a desktop shell runner). The second process should only start when the app server is ready. This plugin and CLIs give you a simple, robust barrier.

## Installation

```
npm i -D @lynx-js/lynxtron-dev-plugins
```

Peer dependency: `@rsbuild/core@^2.1.5`.

## Lynxtron Rsbuild Plugin

Register `pluginLynxtron` in a Node-targeted environment. The plugin applies
the Electron Main target, ESM `__dirname`/`__filename` support, AutoLink,
production minification, and Lynxtron process restart after development
compilations.

```ts
import { defineConfig } from '@rsbuild/core';
import { pluginLynxtron } from '@lynx-js/lynxtron-dev-plugins/rsbuild';

export default defineConfig({
  environments: {
    desktop: {
      source: {
        entry: {
          main: './src/main.ts',
          preload: './src/preload.ts',
        },
      },
      output: {
        target: 'node',
        distPath: {
          root: './dist/desktop',
        },
      },
      dev: {
        writeToDisk: true,
      },
      plugins: [
        pluginLynxtron({
          args: ['--inspect=9222'],
        }),
      ],
    },
  },
});
```

The development entry defaults to the environment output directory. Pass
`entry` to override it, `autolink: false` to disable AutoLink, or
`watchOptions` to replace the default UI-path watch exclusions. `args`, `env`,
and `command` are forwarded to the Lynxtron development process.

## Rspeedy Ready Usage

Register the plugin only for `serve`:

```ts
// rsbuild.config.ts
import { defineConfig } from '@rsbuild/core';
import { pluginRspeedyDevReady } from '@lynx-js/lynxtron-dev-plugins/rspeedy';

export default defineConfig({
  plugins: [pluginRspeedyDevReady()],
});
```

## Rspack Usage

The lower-level Rspack plugin remains available for projects that do not use
Rsbuild. It includes Lynxtron AutoLink by default, so dependencies with
`lynx.lib.json` and a `lynxtron` platform record are staged and loaded
automatically.

For `lynxtron`, `lynx.lib.json` describes the native asset root, such as
`dist`, while the package's `./lynxtron` export provides the JS entry.

```ts
import { pluginLynxtron } from '@lynx-js/lynxtron-dev-plugins/rspack';

export default {
  target: 'electron-main',
  plugins: [
    pluginLynxtron({
      isDev: process.env.NODE_ENV === 'development',
      entry: './dist/desktop',
    }),
  ],
};
```

Set `autolink: false` only when the host intentionally manages native library
loading itself.

Optional configuration:

```ts
pluginRspeedyDevReady({
  // Absolute or relative path to the marker file to write.
  // Defaults to "<dist>/dev-ready.json" based on the first environment.
  markerFile: './output/bundle/dev-ready.json',

  // The line printed to stdout after first successful compile (default "RSBUILD_READY").
  readyLine: 'RSBUILD_READY',

  // The prefix used when printing the dev server port (default "RSBUILD_DEV_SERVER:").
  serverLinePrefix: 'RSBUILD_DEV_SERVER:',
});
```

Behavior reference:

- Ready file path default derives from the first RSBuild environment’s `distPath`.
- On first compile, the plugin writes the file and prints `RSBUILD_READY`.
- On dev server start, it prints the port line.

## CLIs

Use these to block until readiness:

- `dev-ready`

  - Generic waiter for a marker file.
  - Flags:
    - `-f, --file <path>`: file to watch (default `./output/bundle/dev-ready.json`)
    - `-t, --timeout <ms>`: timeout in ms (default `300000`)
  - On success, prints `dev-ready` and exits with `0`.

- `dev-ready-speedy`

  - Waits for RSBuild/RSpeedy marker at `./output/bundle/dev-ready.json`.
  - Requires JSON to contain `{ ready: true, source: "rspeedy" }`.
  - On success, prints `dev-ready-speedy`.

- `dev-ready-rspack`
  - Waits for Rspack marker at `./dist/assets/dev-ready.json`.
  - Requires JSON to contain `{ ready: true, source: "rspack" }`.
  - On success, prints `dev-ready-rspack`.

All CLIs are ES modules with a shebang and must be executed by Node-capable shells (the package provides proper `#!/usr/bin/env node`).

## Example: Coordinated Dev

Run RSBuild and a dependent process in parallel:

```
{
  "scripts": {
    "dev:lynx": "rspeedy dev",
    "dev:shell": "dev-ready-speedy && my-shell --inspect=9222 ./dist/assets",
    "dev": "npm-run-all -l dev:lynx dev:shell"
  }
}
```

`dev:shell` starts only after RSBuild writes `dev-ready.json` and the CLI returns success.

## Internals

- Plugin implementation: `pluginRspeedyDevReady` writes the marker on first compile and logs port info on server start (`packages/lynxtron-dev-plugins/src/rspeedy.ts:1`).
- CLIs poll the expected file until it contains a valid JSON and required fields, then exit (`packages/lynxtron-dev-plugins/bin/cli-rspeedy.js:1`).

## Troubleshooting

- Seeing “command not found” for `import`?
  - Ensure you call the provided CLIs (which include a shebang), or run via `node` explicitly.
- CLI times out:
  - Verify the file path and contents match the expected `source` (`rspeedy` or `rspack`).
  - Increase timeout with `-t <ms>` for `dev-ready`.
