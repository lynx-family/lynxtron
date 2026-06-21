#!/usr/bin/env python3
# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import os
import shutil
import sys
from pathlib import Path


# Automatically determine the repository root (lynxtron directory)
project_root = Path(__file__).resolve().parent.parent
print(f"Repository root: {project_root}")

# Define directories
node_source_dir = project_root / "third_party" / "node"
v8_source_dir = project_root / "v8"
target_dir = project_root / "out" / "Release" / "node_headers" / "include" / "node"

# 1. Delete the old directory
print("Step 1: Deleting old node_headers directory...")
node_headers_root = project_root / "out" / "Release" / "node_headers"
if node_headers_root.exists():
    shutil.rmtree(node_headers_root)
    print("  Old directory deleted")

# 2. Create the new directory
print("\nStep 2: Creating new directory structure...")
target_dir.mkdir(parents=True, exist_ok=True)
print(f"  Created: {target_dir}")

# 3. Copy Node.js core headers
print("\nStep 3: Copying Node.js core headers...")
node_core_headers = [
    "src/node.h",
    "src/node_api.h",
    "src/js_native_api.h",
    "src/js_native_api_types.h",
    "src/node_api_types.h",
    "src/node_buffer.h",
    "src/node_object_wrap.h",
    "src/node_version.h",
    "common.gypi"
]
for header in node_core_headers:
    src = node_source_dir / header
    if src.exists():
        shutil.copy2(src, target_dir / os.path.basename(header))
        print(f"  Copied: {header}")
    else:
        print(f"  Warning: {header} not found")

# 4. Copy config.gypi
print("\nStep 4: Copying config.gypi...")
# Try looking under out/Release/gen/node_headers
gen_config = project_root / "out" / "Release" / "gen" / "node_headers" / "include" / "node" / "config.gypi"
if gen_config.exists():
    shutil.copy2(gen_config, target_dir / "config.gypi")
    print("  Copied: config.gypi from gen/node_headers")
else:
    print("  Warning: config.gypi not found")

# 5. Copy V8 headers
print("\nStep 5: Copying V8 headers...")
v8_include_dir = v8_source_dir / "include"
if v8_include_dir.exists():
    # Copy all .h files
    for item in v8_include_dir.iterdir():
        if item.is_file() and item.suffix == ".h":
            shutil.copy2(item, target_dir / item.name)
            print(f"  Copied V8 header: {item.name}")
        elif item.is_dir():
            # Copy subdirectories
            dest_subdir = target_dir / item.name
            shutil.copytree(item, dest_subdir, dirs_exist_ok=True)
            print(f"  Copied V8 directory: {item.name}/")

# 6. Copy libuv headers
print("\nStep 6: Copying libuv headers...")
uv_include_dir = node_source_dir / "deps" / "uv" / "include"
if uv_include_dir.exists():
    for item in uv_include_dir.iterdir():
        if item.is_file() and item.suffix == ".h":
            shutil.copy2(item, target_dir / item.name)
            print(f"  Copied uv header: {item.name}")
        elif item.is_dir():
            dest_subdir = target_dir / item.name
            shutil.copytree(item, dest_subdir, dirs_exist_ok=True)
            print(f"  Copied uv directory: {item.name}/")

# 7. Copy zlib headers
print("\nStep 7: Copying zlib headers...")
zlib_dir = node_source_dir / "deps" / "zlib"
zlib_headers = ["zlib.h", "zconf.h"]
for header in zlib_headers:
    src = zlib_dir / header
    if src.exists():
        shutil.copy2(src, target_dir / header)
        print(f"  Copied zlib header: {header}")

print("\n=== Completed ===")
print(f"Headers copied to: {target_dir}")
print(f"\nDirectory contents:")
for item in sorted(target_dir.iterdir()):
    prefix = "[DIR]  " if item.is_dir() else "[FILE] "
    print(f"  {prefix}{item.name}")
