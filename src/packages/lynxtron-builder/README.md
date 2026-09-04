# Lynxtron Builder

`lynxtron-builder` packages applications with the production runtime by default. The installed `lynxtron` CLI uses the DevTool-enabled runtime by default, so the normal development and packaging commands do not require different npm versions.

To package a DevTool-enabled application, add this package-specific section to `electron-builder.yml`:

```yaml
lynxtron:
  runtimeVariant: devtool
```

The builder consumes this section before invoking electron-builder. Valid variants are `release` and `devtool`.

You can also override the variant for one command:

```bash
lynxtron-builder --lynxtron-runtime=devtool --mac
```

Selection precedence is command line, `LYNXTRON_RUNTIME_VARIANT`, `electron-builder.yml`, then the `release` default. By default, the builder downloads the matching versioned runtime from the Lynxtron GitHub release. An explicitly configured `electronDownload` remains authoritative.

## AutoLink native packages

`pluginLynxtron()` stages matching native packages under
`.lynxtron/native`. The builder includes that directory in the final app and
keeps it outside ASAR so native addons and adjacent runtime files remain
loadable.

For macOS targets, Frameworks and nested applications declared by the selected
`platforms.lynxtron.targets` record are copied into `Contents/Frameworks` with
their symbolic links preserved. Nested applications use `appBundles` and are
included before electron-builder signs the outer application. For Windows
targets, declared `files` stay under `app.asar.unpacked`, including the selected
`.node` addon, DLLs, resource packs, and locales.
