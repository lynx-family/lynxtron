# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import shutil
import sys

def ensure_dir(path):
    os.makedirs(path, exist_ok=True)

def remove_path(path):
    if os.path.isdir(path) and not os.path.islink(path):
        shutil.rmtree(path)
    elif os.path.exists(path):
        os.remove(path)

def copy_app(src_app, dest_dir):
    ensure_dir(dest_dir)
    dest_app = os.path.join(dest_dir, os.path.basename(src_app))
    remove_path(dest_app)
    shutil.copytree(src_app, dest_app, symlinks=True)
    return dest_app

def touch(path):
    dir_name = os.path.dirname(path)
    if dir_name:
        ensure_dir(dir_name)
    with open(path, "w") as f:
        f.write("")

def main():
    if len(sys.argv) != 4:
        print("Usage: copy_to_package.py <src_app> <dest_dir> <marker_file>")
        return 1
    src_app = sys.argv[1]
    dest_dir = sys.argv[2]
    marker_file = sys.argv[3]
    if not os.path.isdir(src_app):
        print(f"Source app not found: {src_app}")
        return 1
    try:
        copy_app(src_app, dest_dir)
        touch(marker_file)
        return 0
    except Exception as e:
        print(f"Copy failed: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
