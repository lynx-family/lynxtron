# @lynx-js/lynx-library-headers

Header package for Lynx embedder C/C++ API and weak N-API based native
libraries. This package provides headers and CMake helpers; it does not
scaffold, build, or register libraries.

## Usage

Resolve the package root from your build script, then add `include/` to the
native target include directories:

```js
const path = require('node:path');
const headersRoot = path.dirname(
  require.resolve('@lynx-js/lynx-library-headers/package.json')
);
```

For Windows Lynx native library builds, include the packaged CMake helper to
resolve the host runtime import library used by the generated native target:

```cmake
include("${headersRoot}/cmake/lynx-library-headers.cmake")
lynx_link_lynxtron_runtime(your_target)
```

The helper resolves the Windows import library from the runtime package
installed with `@lynx-js/lynx-library-headers`. You can override discovery with
`-DLYNXTRON_IMPORT_LIB=/path/to/lynxtron.dll.lib`.

The published package contains:

- Lynx embedder C API headers.
- Lynx embedder C++ wrapper headers such as `lynx_native_view.h`,
  `lynx_value.h`, and `lynx_view.h`.
- Weak N-API headers used by Lynx native modules.
- Lynx value C API headers required by the embedder C API.
- `lynx_extension.h` and `lynx/extension.h` convenience wrapper headers.
- `lynx/registration.h` for generic embedder static registration helpers.
- `cmake/lynx-library-headers.cmake` for resolving Windows runtime import
  libraries used by Lynx native library builds.

Lynx native libraries use static registration from the native library:

```cpp
#include <lynx/registration.h>

void BindDemoModule(::lynx::registration::LynxNapiEnv env,
                    ::lynx::registration::LynxNapiValue exports);
lynx_native_view_t* CreateDemoView(void* opaque);

LYNX_REGISTER_NATIVE_MODULE("DemoModule", BindDemoModule)
LYNX_REGISTER_ELEMENT(
    "DemoModule",
    "x-demo-view",
    CreateDemoView)
```

Use `LYNX_REGISTER_NATIVE_MODULE` for Lynx weak N-API native modules and
`LYNX_REGISTER_ELEMENT` for Lynx native view elements. A library that exposes
both features should call both macros, keeping the native module and element
registrations separate.

A host runtime that loads the native library is enough for these static
registrations to run.

Run `npm run copy-headers` in this package when updating the source headers.
