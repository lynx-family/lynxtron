# How to Build

Currently supports macOS and Windows.

Dependencies:

- Node.js >= 22.18.0
- Python 3
- Xcode >= 15.2 (macOS)
- Visual Studio 2022 (Windows)
- Windows 11 SDK version 10.0.26100 with Debugging Tools for Windows installed (Windows)

## Windows Line Endings (Required)

Before cloning on Windows, configure Git to use LF endings to avoid CRLF issues:

```
git config --global core.autocrlf false
git config --global core.eol lf
```

## Build Steps

### macOS

```
git clone git@github.com:lynx-family/lynxtron.git
cd lynxtron
source lynxtron_tools/envsetup.sh
python3 lynxtron_tools/prepare_build_env.py
# release build
python3 lynxtron_tools/gn/gn.py --mac-cpu ['x64', 'arm64']
# release build with trace
python3 lynxtron_tools/gn/gn.py --enable-trace --mac-cpu ['x64', 'arm64']
ninja -C out/Release lynxtron_app
# debug build
python3 lynxtron_tools/gn/gn.py --is-debug --mac-cpu ['x64', 'arm64']
# debug build with trace
python3 lynxtron_tools/gn/gn.py --enable-trace --is-debug --mac-cpu ['x64', 'arm64']
ninja -C out/Debug lynxtron_app
```

### Windows (PowerShell)

```
git clone git@github.com:lynx-family/lynxtron.git
cd lynxtron
lynxtron_tools/envsetup.ps1
python3 lynxtron_tools/prepare_build_env.py
$env:DEPOT_TOOLS_WIN_TOOLCHAIN=0
# release build
python lynxtron_tools/gn/gn.py --windows-cpu ['x64', 'x86']
# release build with trace
python lynxtron_tools/gn/gn.py --enable-trace --windows-cpu ['x64', 'x86']
ninja -C out/Release lynxtron_app
# debug build
python lynxtron_tools/gn/gn.py --is-debug --windows-cpu ['x64', 'x86']
# debug build with trace
python lynxtron_tools/gn/gn.py --enable-trace --is-debug --windows-cpu ['x64', 'x86']
ninja -C out/Debug lynxtron_app
```

# Windows GPU preference

Source builds retain the high-performance GPU hint by default. To leave GPU
selection to Windows, build with
`--gn-args 'lynxtron_prefer_discrete_gpu=false use_discrete_gpu=false'`.
For a matching CEF helper build, set `LYNXTRON_PREFER_DISCRETE_GPU=false` in the
build environment; it defaults to `true` for source builds.

The GitHub `publish` workflow exposes `prefer_discrete_gpu` (default `false`)
for both manual and reusable calls. It applies to Release, DevTools, and the
Windows CEF helper. Enabling it exports the NVIDIA/AMD high-performance hints;
disabling it omits them. Windows per-application graphics settings can still
select a GPU. This option does not force integrated graphics or affect macOS.

# Formatting

Format code before committing:

```
cd src/lynxtron
# macOS
source lynxtron_tools/envsetup.sh
# Windows PowerShell
lynxtron_tools/envsetup.ps1

git lynx format --changed
```

If you forgot to format before committing, modify the problematic files slightly and rerun the command above.

To format the entire repo:

```
git lynx format --all
```

To run specific checks (currently supported: `coding-style`, `cpplint`):

```
git lynx check --checkers xxx,yyy
```
