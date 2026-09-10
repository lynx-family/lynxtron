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
