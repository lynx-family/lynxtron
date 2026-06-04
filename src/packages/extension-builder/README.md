# Lynx Extension Builder

This directory contains tools and utilities for building Lynx extensions. It provides a framework for creating custom extensions that can be integrated with Lynx runtime.

## Directory Structure

```
extension_builder/
├── bindings/           # NAPI bindings for extension development
│   └── bind_napi.cc    # Main NAPI binding implementation
├── module/             # Example extension module
│   ├── demo_extension_module.cc  # Example extension module implementation
│   ├── demo_extension_module.h  # Example extension module header
│   ├── demo_view.cc             # Example view implementation
│   └── demo_view.h             # Example view header
├── scripts/            # Utility scripts
│   └── prepare.js      # Preparation script for extension building
├── .gitignore          # Git ignore file
├── CMakeLists.txt      # CMake build configuration
├── index.cjs           # Main entry point for extension builder
├── package.json        # Node.js package configuration
└── README.md           # This documentation file
```

## Features

- **Extension Module Framework**: Provides a template for creating custom Lynx extensions
- **NAPI Bindings**: Enables JavaScript-native code interaction
- **Example Implementation**: Includes demo extension module and view for reference
- **Build System**: Uses CMake for cross-platform building

## License

This extension builder is part of the Lynx project. Refer to the project's main license file for licensing information.
