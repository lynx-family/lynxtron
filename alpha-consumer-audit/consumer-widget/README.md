# @alpha-consumer/widget

Native Lynx library.

## Development

```bash
npm install
npm run codegen
```

This feature selection does not include native module typings.
Selected native files are written to `lynxtron/` and `shared/`. The package
is discovered by Lynx through `lynx.lib.json`.


## Lynxtron Library Target

This package contains shared C++ sources for selected Native Module, NAPI Native
Module, or Element features. Build the current OS/architecture Lynxtron library
with:

```bash
npm run build:lynxtron
```

The build writes `dist/<platform>/<arch>/widget.node`.
Run it on each OS/architecture you want to publish. The package also exposes
`./lynxtron`, which loads the matching artifact for Lynxtron based hosts.
`npm pack` and `npm publish` do not build native artifacts, so collect every
supported platform/architecture under `dist/` before publishing.

In the Lynxtron Node.js main thread:

```cjs
const addon = require('@alpha-consumer/widget/lynxtron');
addon.initialize();
```


