# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import os
import shutil

# Get script directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# Define target directory
NPM_PACKAGE_DIR = os.path.join(SCRIPT_DIR, "npm_package")
# Define source files and directories
SOURCE_FILES = ["package.json", "index.cjs", "README.md"]
SOURCE_DIRS = ["scripts", "packages"]

def main():
    # 1. Create temporary directory npm_package
    if os.path.exists(NPM_PACKAGE_DIR):
        shutil.rmtree(NPM_PACKAGE_DIR)
    os.makedirs(NPM_PACKAGE_DIR)

    # 2. Copy package.json and index.cjs
    for file_name in SOURCE_FILES:
        src_path = os.path.join(SCRIPT_DIR, file_name)
        dest_path = os.path.join(NPM_PACKAGE_DIR, file_name)
        if os.path.exists(src_path):
            shutil.copy2(src_path, dest_path)
        else:
            print(f"Warning: {src_path} does not exist!")

    # 3. Copy scripts directory and its files
    for dir_name in SOURCE_DIRS:
        src_path = os.path.join(SCRIPT_DIR, dir_name)
        dest_path = os.path.join(NPM_PACKAGE_DIR, dir_name)
        if os.path.exists(src_path):
            shutil.copytree(src_path, dest_path)
        else:
            print(f"Warning: {src_path} does not exist!")

    print("\nPackaging completed!")


if __name__ == "__main__":
    main()
