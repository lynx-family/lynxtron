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

Selection precedence is command line, `LYNXTRON_RUNTIME_VARIANT`, `electron-builder.yml`, then the `release` default. An explicitly configured `electronDownload` remains authoritative.

## AutoLink native packages

`pluginLynxtron()` stages matching native packages under
`.lynxtron/native`. The builder includes that directory in the final app and
keeps it outside ASAR so native addons and adjacent runtime files remain
loadable.

Every `lynx.lib.json` target must use literal package-relative artifact paths
and standard `os` (`darwin`, `win32`, `linux`) and `arch` (`arm64`, `x64`, `ia32`)
names. Variables, globs, and aliases are rejected. The builder selects the
packaging target from the original staged manifest; it does not expand or
rewrite that manifest.

For macOS targets, Frameworks and nested applications declared by the selected
`platforms.lynxtron.targets` record are copied into `Contents/Frameworks` with
their symbolic links preserved. Nested applications use `appBundles` and are
included before electron-builder signs the outer application. For Windows
targets, declared `files` stay under `app.asar.unpacked`, including the selected
`.node` addon, DLLs, resource packs, and locales.
