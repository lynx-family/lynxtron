# How to Build

Currently supports macOS and Windows.

Dependencies:

- Node.js >= 22
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
python3 lynxtron_tools/prepare_build_env.py
source lynxtron_tools/envsetup.sh
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
python3 lynxtron_tools/prepare_build_env.py
lynxtron_tools/envsetup.ps1
$env:DEPOT_TOOLS_WIN_TOOLCHAIN=0
# release build
python lynxtron/tools/gn/gn.py --windows-cpu ['x64', 'x86']
# release build with trace
python lynxtron/tools/gn/gn.py --enable-trace --windows-cpu ['x64', 'x86']
ninja -C out/Release lynxtron_app
# debug build
python lynxtron/tools/gn/gn.py --is-debug --windows-cpu ['x64', 'x86']
# debug build with trace
python lynxtron/tools/gn/gn.py --enable-trace --is-debug --windows-cpu ['x64', 'x86']
ninja -C out/Debug lynxtron_app
```

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

