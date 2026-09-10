# Lynxtron HarmonyOS application

This directory contains the HarmonyOS HAP wrapper for Lynxtron. The ArkTS
UIAbility loads `liblynxtron_napi.so`, which starts `liblynxtron.so` and binds
an XComponent surface to the Lynx renderer.

The arm64 cross-build and HAP have been verified on macOS and run on a
HarmonyOS PC. The native build can also be driven from Linux when equivalent
host tools are installed.

## Prerequisites

The build uses two separate tool installations:

1. A HarmonyOS Native SDK providing the target sysroot, target libraries and
   compiler runtime.
2. A host-native LLVM toolchain used to execute `clang`, `clang++` and the LLVM
   binary utilities on the build machine.

Do not use the old Clang 15 executable shipped by some SDK packages as the
host compiler. This source tree requires **Clang 19 or newer**. The target
compiler-runtime version is independent of the host Clang version and must
match the directory actually present in the HarmonyOS Native SDK.

The complete requirements are:

- macOS arm64 or Linux x64 build host;
- Python 3 and Git;
- Node.js and package tools installed by the dependency preparation script;
- a host-native LLVM/Clang **19 or newer** distribution containing `clang`,
  `clang++`, `llvm-ar`, `llvm-nm`, `llvm-readelf`, `llvm-objcopy` and
  `llvm-strip`;
- HarmonyOS Native SDK **6.1.1 / API 24**;
- HarmonyOS Command Line Tools providing an API 24 SDK, `hvigorw`, `ohpm`,
  Node.js and `hap-sign-tool.jar`;
- JDK 17 for HAP assembly and signing;
- `hdc` for installing and launching the HAP on a device.

## Recommended directory layout

The Native SDK and Command Line Tools may be installed anywhere. Keep them
outside the checkout and pass absolute paths to the build:

```text
/path/to/ohos/
├── sdk-root/
│   └── HarmonyOS-NEXT-DB1/
│       └── openharmony/native/
│           ├── sysroot/
│           └── llvm/
│               ├── include/libcxx-ohos/include/c++/v1/
│               ├── lib/aarch64-linux-ohos/
│               └── lib/clang/15.0.4/   # example; inspect your SDK
├── LLVM-19.1.7-host/
│   └── bin/
│       ├── clang
│       ├── clang++
│       └── llvm-*
└── command-line-tools/
    ├── version.txt
    ├── bin/hvigorw
    ├── ohpm/bin/ohpm
    ├── tool/node/bin/node
    └── sdk/default/openharmony/toolchains/lib/hap-sign-tool.jar
```

The Native SDK may be the SDK bundled with Command Line Tools when that tree
contains `openharmony/native`; the two are listed separately because they play
different roles and are often installed from different packages.

The name `HarmonyOS-NEXT-DB1` is the GN default, not a required on-disk name.
Set `harmony_sdk_version` to the directory name you use. If `sdk-root` itself
contains `openharmony/native`, set `harmony_sdk_version=""`.

Before generating the build, verify both versions rather than assuming they
are the same:

```sh
/path/to/ohos/LLVM-19.1.7-host/bin/clang++ --version
find /path/to/ohos/sdk-root/HarmonyOS-NEXT-DB1/openharmony/native/llvm/lib/clang \
  -mindepth 1 -maxdepth 1 -type d -print
```

For example, a host Clang 19 installation can legitimately be paired with an
SDK whose target runtime is stored under `lib/clang/15.0.4`.

## Prepare the checkout

Clone or update the intended branch, then synchronize every pinned dependency
and apply the repository's patch series:

```sh
git clone https://github.com/lynx-family/lynxtron.git
cd lynxtron
# Check out the branch containing HarmonyOS support before continuing.

python3 lynxtron_tools/prepare_build_env.py
source lynxtron_tools/envsetup.sh
```

Run `prepare_build_env.py` again after changing branches or pulling a revision
that updates a `DEPS` file or anything under `src/patches`. Do not copy
`node_modules`, `lynx`, `build`, `buildtools` or `third_party` directories
between Linux and macOS checkouts; host-specific binaries such as SWC will be
wrong. If GN reports a missing file below `//lynx`, dependency preparation is
the first thing to rerun.

## Build the native libraries

The examples below build an arm64 Release HAP. Substitute your actual absolute
paths. `harmony_compiler_rt_version` must be the directory name discovered in
the Native SDK, not the host Clang version.

```sh
export LYNXTRON_OHOS_SDK_ROOT=/path/to/ohos/sdk-root
export LYNXTRON_OHOS_SDK_VERSION=HarmonyOS-NEXT-DB1
export LYNXTRON_OHOS_HOST_LLVM=/path/to/ohos/LLVM-19.1.7-host
export LYNXTRON_OHOS_COMPILER_RT_VERSION=15.0.4

python3 lynxtron_tools/gn/gn.py \
  --target-os=harmony \
  --harmony-cpu=arm64 \
  --gn-args="harmony_sdk_root=\"${LYNXTRON_OHOS_SDK_ROOT}\" \
harmony_sdk_version=\"${LYNXTRON_OHOS_SDK_VERSION}\" \
harmony_host_toolchain_root=\"${LYNXTRON_OHOS_HOST_LLVM}\" \
harmony_compiler_rt_version=\"${LYNXTRON_OHOS_COMPILER_RT_VERSION}\" \
use_custom_libcxx=true"

ninja -C out/harmony_arm64_Release -j6 \
  lynxtron_app lynxtron_napi_bridge default_app_asar
```

`use_custom_libcxx=true` is required with the verified Clang 19 setup. It uses
Chromium's newer libc++ headers while retaining the HarmonyOS SDK's target
sysroot and libraries. Mixing the older SDK libc++ headers with Clang 19 can
produce missing C++20 ranges APIs and libc locale-function errors.

Successful output includes:

```text
out/harmony_arm64_Release/liblynxtron.so
out/harmony_arm64_Release/liblynxtron_napi.so
out/harmony_arm64_Release/icudtl.dat
out/harmony_arm64_Release/snapshot_blob.bin
out/harmony_arm64_Release/resources/default_app.asar
```

For a Debug build, add `--is-debug`; the output directory becomes
`out/harmony_arm64_Debug`. The current HAP helper stages Release output, so use
Release unless you also adjust `GN_OUT_DIR` in `build_hap.sh`.

## Assemble the HAP

Point the packaging script at a Command Line Tools installation whose
`version.txt` reports API 24 or newer, and make JDK 17 available:

```sh
export JAVA_HOME=/path/to/jdk-17
export COMMAND_LINE_TOOLS=/path/to/ohos/command-line-tools

./harmony_app/build_hap.sh
```

The script stages the native libraries and runtime resources, installs the
OHPM dependencies, and produces:

```text
harmony_app/entry/build/default/outputs/default/entry-default-unsigned.hap
```

`COMMAND_LINE_TOOLS` may be omitted when the tools are below a directory named
`command-line-tools` under the repository, its parent, or `OHOS_TOOLS`.
Setting it explicitly is recommended for reproducible builds.

## Sign the HAP

The configured application bundle name is `com.huawei.electron`. The signing
profile must contain exactly that bundle name and must authorize the target
device. Keep certificates, keystores and passwords outside the repository.

Create an untracked `harmony_app/signing.local.env`, or point
`SIGN_ENV_FILE` at an equivalent file outside the checkout:

```sh
SIGN_CERT_DIR=/absolute/path/to/certificates
SIGN_KEY_ALIAS=your-key-alias
SIGN_CERT_FILE=your-certificate.cer
SIGN_PROFILE_FILE=your-device-profile.p7b
SIGN_KEYSTORE_FILE=your-keystore.p12
SIGN_KEY_PWD=your-key-password
SIGN_KEYSTORE_PWD=your-keystore-password
```

Then assemble and sign in one command:

```sh
export JAVA_HOME=/path/to/jdk-17
export COMMAND_LINE_TOOLS=/path/to/ohos/command-line-tools
./harmony_app/build_hap.sh --signed
```

The signed package is written to:

```text
harmony_app/entry/build/default/outputs/default/lynxtron-default-signed.hap
```

## Install and run

Enable developer mode on the HarmonyOS device and verify that `hdc` can see it:

```sh
hdc list targets
```

With one connected device:

```sh
hdc install -r \
  harmony_app/entry/build/default/outputs/default/lynxtron-default-signed.hap
hdc shell aa start -a EntryAbility -b com.huawei.electron
```

With multiple connected devices, select one explicitly:

```sh
export HDC_TARGET=your-device-serial
hdc -t "${HDC_TARGET}" install -r \
  harmony_app/entry/build/default/outputs/default/lynxtron-default-signed.hap
hdc -t "${HDC_TARGET}" shell aa start \
  -a EntryAbility -b com.huawei.electron
```

Useful diagnostics:

```sh
hdc shell aa force-stop com.huawei.electron
hdc shell hilog -x | grep -E 'Lynxtron|LynxtronMain|LynxtronBridge|LynxtronWLR'
```

Lynxtron also writes its native log inside the application sandbox at:

```text
/data/storage/el2/base/haps/entry/files/lynxtron_debug.log
```

## Troubleshooting

### Clang 15 fails or C++20 APIs are missing

Use a host-native Clang 19 or newer, pass it through
`harmony_host_toolchain_root`, and generate with `use_custom_libcxx=true`.
Keep `harmony_compiler_rt_version` set to the runtime directory shipped in the
target SDK.

### GN cannot load a file under `//lynx`

The pinned dependencies are stale or were only partially synchronized. From
the repository root, rerun:

```sh
python3 lynxtron_tools/prepare_build_env.py
source lynxtron_tools/envsetup.sh
```

### `SDK component missing` while assembling the HAP

The selected Command Line Tools installation is older than the project's
`compileSdkVersion`. Set `COMMAND_LINE_TOOLS` to a release providing API 24 or
newer and rerun `build_hap.sh`.

### Signing or installation rejects the package

Check that the signing profile contains `com.huawei.electron`, includes the
device UDID when required, and matches the certificate and keystore. Do not
change `bundleName` merely to bypass a signing failure; obtain a matching
profile instead.

## Source layout

```text
harmony_app/
├── AppScope/app.json5          # application identity and resources
├── build-profile.json5        # API level and products
├── build_hap.sh               # stages, assembles and optionally signs
└── entry/
    ├── build-profile.json5
    ├── libs/arm64-v8a/         # generated native libraries
    └── src/main/
        ├── module.json5        # EntryAbility metadata and permissions
        ├── ets/                # ArkTS UIAbility and XComponent UI
        ├── cpp/                # NAPI bridge module definition
        └── resources/resfile/  # generated runtime resources
```

Generated native libraries, HAPs, runtime resources and local signing settings
are intentionally excluded from version control.
