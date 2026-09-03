# Lynxtron HarmonyOS application

This directory contains the HarmonyOS HAP wrapper for Lynxtron. The ArkTS
UIAbility loads a small NAPI bridge, which starts `liblynxtron.so` and connects
an XComponent surface to the Lynx renderer.

The arm64 cross build and HAP have been verified on a HarmonyOS PC. Device
installation requires a local signing certificate and a profile matching the
configured bundle name.

## Layout

```
harmony_app/
├── AppScope/              # workspace-level app metadata
│   ├── app.json5          # application metadata
│   └── resources/
├── build-profile.json5    # workspace build settings
├── hvigorfile.ts          # workspace tasks
├── oh-package.json5       # workspace deps (hvigor / hvigor-ohos-plugin)
└── entry/                 # main HAP module
    ├── build-profile.json5
    ├── hvigorfile.ts
    ├── oh-package.json5
    └── src/main/
        ├── module.json5             # module + EntryAbility metadata
        ├── ets/
        │   ├── entryability/
        │   │   └── EntryAbility.ets # UIAbility lifecycle
        │   └── pages/
        │       └── Index.ets        # main UI; calls lynxtron.start()
        ├── cpp/
        │   ├── CMakeLists.txt       # imports prebuilt liblynxtron.so
        │   └── types/libentry/
        │       ├── Index.d.ts       # ts type decl for `import lynxtron`
        │       └── oh-package.json5
        ├── resources/
        │   └── base/
        │       ├── element/string.json
        │       ├── element/color.json
        │       └── profile/main_pages.json
        └── rawfile/                 # placeholder (icons / assets)
```

## Build

Prepare the regular Lynxtron dependencies first:

```sh
python3 lynxtron_tools/prepare_build_env.py
```

Make the HarmonyOS SDK available either at the repository-local default path
or through `HARMONY_HOME`. The expected layout is:

```text
$HARMONY_HOME/HarmonyOS-NEXT-DB1/openharmony/native/llvm/bin/clang
```

For example, if the downloaded SDK directory itself contains `openharmony/`,
create a versioned link outside the repository and export its parent:

```sh
mkdir -p /path/to/harmony-sdk-root
ln -s /path/to/downloaded-sdk \
  /path/to/harmony-sdk-root/HarmonyOS-NEXT-DB1
export HARMONY_HOME=/path/to/harmony-sdk-root
```

Then generate and build the HarmonyOS arm64 targets from the repository root:

```sh
python3 lynxtron_tools/gn/gn.py --target-os=harmony --harmony-cpu=arm64
ninja -C out/harmony_arm64_Release \
  lynxtron_app lynxtron_napi_bridge default_app_asar
```

Build the HAP after installing the HarmonyOS command-line tools:

```sh
./harmony_app/build_hap.sh
```

The unsigned package is written to:

```text
harmony_app/entry/build/default/outputs/default/entry-default-unsigned.hap
```

`build_hap.sh --signed` can sign the result when all signing environment
variables documented by the script are set. Certificates, profiles, key stores,
and passwords must remain outside the repository.

## Running on device

```sh
hdc install harmony_app/entry/build/default/outputs/default/lynxtron-default-signed.hap
hdc shell aa start -a EntryAbility -b com.lynxtron.harmony
hdc hilog -T Lynxtron,LynxtronMain,LynxtronBridge,LynxtronWLR,LynxtronRun
```

The Harmony build also writes Chromium/Lynx logs to the app sandbox:

```sh
/data/storage/el2/base/haps/entry/files/lynxtron_debug.log
```

## Notes

- `build_hap.sh` stages native libraries and runtime resources from
  `out/harmony_arm64_Release`; generated files are intentionally ignored.
- The NAPI bridge runs `LynxtronMain` on a dedicated thread so the ArkUI event
  loop remains responsive.
- Change `bundleName` only when the signing profile is updated to match it.
