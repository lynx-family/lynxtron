#!/usr/bin/env python3
# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import os
import sys


SOURCE_ROOT = os.path.dirname(os.path.dirname(__file__))


def dependency_path(node_modules_dir, name):
    return os.path.join(node_modules_dir, *name.split("/"), "package.json")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", default=SOURCE_ROOT)
    parser.add_argument("--silent", action="store_true")
    parser.add_argument("--stamp")
    args = parser.parse_args()

    package_json_path = os.path.join(args.directory, "package.json")
    try:
        with open(package_json_path, "r", encoding="utf-8") as package_json_file:
            package_json = json.load(package_json_file)
    except OSError as error:
        print(f"Could not read package json in {args.directory}: {error}")
        return 1
    except json.JSONDecodeError as error:
        print(f"The package.json file found did not contain valid JSON: {error}")
        return 1

    node_modules_dir = os.path.join(args.directory, "node_modules")
    missing = []
    for deps_key, optional in (
        ("dependencies", False),
        ("devDependencies", False),
        ("optionalDependencies", True),
    ):
        for dep_name in package_json.get(deps_key, {}):
            if os.path.exists(dependency_path(node_modules_dir, dep_name)):
                continue
            if not optional:
                missing.append(dep_name)

    if missing:
        for dep_name in missing:
            print(f"Could not find required dependency: {dep_name}")
        if args.stamp and os.path.exists(args.stamp):
            os.unlink(args.stamp)
        return 1

    if not args.silent:
        print("Dependencies are installed, looking good!")

    if args.stamp:
        os.makedirs(os.path.dirname(args.stamp), exist_ok=True)
        with open(args.stamp, "w", encoding="utf-8") as stamp_file:
            stamp_file.write("pre-flight-stamp")
    return 0


if __name__ == "__main__":
    sys.exit(main())
