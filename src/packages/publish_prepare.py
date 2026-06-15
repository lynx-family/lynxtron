# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
#
# publish_prepare.py
#
# Prepare-only counterpart of //packages/publish.py for the open source
# publishing pipeline.
#
# Differences from publish.py:
#   1. It does NOT execute the final `npm publish` step (no publish_module).
#   2. It only takes a tag (and version) as input, updates version numbers in
#      each module's package.json, and optionally runs prepare/build scripts
#      defined per module so that the workspace is ready for an external
#      `npm publish` step performed by the pipeline.
#
# Usage:
#   python3 publish_prepare.py --version 0.1.0 --tag latest [--module ALL]
import argparse
import json
import os
import subprocess
from pathlib import Path


# Fields:
#   package_dir:     Path to the package, relative to this script's directory.
#   replaces:        Optional. Map of dependency name -> { name, version }.
#                    Used to bump cross-package workspace dependencies (e.g.
#                    `workspace:*`) to the concrete version being published.
#                    If `version` is empty, args.version is used.
#                    The dependency name is NOT renamed (kept as @lynx-js/*).
#   prepare_script:  Optional. Command (list) executed before build.
#   build_script:    Optional. Command (list) executed to build the module.
#   build_pass_args: Optional. If true, append `--version` / `--tag` to build.
PREPARE_CONFIG = {
    "lynxtron": {
        "package_dir": "./lynxtron",
    },
    "create-lynxtron": {
        "package_dir": "./create-lynxtron",
        "replaces": {
            "@lynx-js/cef-x-webview": {
                "name": "@lynx-js/cef-x-webview",
                "version": "",
            },
        },
    },
    "create-browser-demo": {
        "package_dir": "./create-browser-demo",
        "replaces": {
            "@lynx-js/cef-x-webview": {
                "name": "@lynx-js/cef-x-webview",
                "version": "",
            },
        },
    },
    "lynxtron-builder": {
        "package_dir": "./lynxtron-builder",
    },
    "lynxtron-rebuild": {
        "package_dir": "./lynxtron-rebuild",
    },
    "lynxtron-dev-plugins": {
        "package_dir": "./lynxtron-dev-plugins",
        "build_script": ["npm", "run", "build"],
    },
    "cef-x-webview": {
        "package_dir": "./cef-x-webview",
    },
    "extension-builder": {
        "package_dir": "./extension-builder",
        "replaces": {
            "@lynx-js/lynxtron": {
                "name": "@lynx-js/lynxtron",
                "version": "",
            },
        },
    },
}


def args_parser():
    parser = argparse.ArgumentParser(prog="publish_prepare.py")
    parser.add_argument("--version", required=True,
                        help="Version to write into each package.json.")
    parser.add_argument("--tag", required=True,
                        help="Dist tag, forwarded to build_script when "
                             "build_pass_args is true.")
    parser.add_argument("--module", required=False, default="ALL",
                        help="Single module key to prepare. Defaults to ALL.")
    return parser.parse_args()


# @param modules: list of module names
def module_init(modules, packages_dir, cfg, args):
    for module in modules:
        module_cfg = cfg.get(module)
        if not module_cfg:
            raise RuntimeError(
                f"module not found in PREPARE_CONFIG: {module}")
        pkg_dir_cfg = module_cfg.get("package_dir")
        target_dir = (packages_dir / pkg_dir_cfg).resolve()

        # 1. Update version in package.json. Package name is kept untouched.
        pkg_path = target_dir / "package.json"
        with pkg_path.open("r", encoding="utf-8") as f:
            pkg = json.load(f)
        pkg["version"] = args.version

        # 2. Bump cross-package dependency versions according to `replaces`.
        #    The dependency name is NOT renamed; only the version is updated.
        replaces = module_cfg.get("replaces")
        if replaces:
            for deps_key in ("dependencies", "devDependencies"):
                if deps_key not in pkg:
                    continue
                deps = pkg[deps_key]
                for old_pkg, new_pkg_info in replaces.items():
                    if old_pkg not in deps:
                        continue
                    new_name = new_pkg_info["name"]
                    new_version = new_pkg_info["version"]
                    if new_version == "":
                        new_version = args.version
                    if new_name != old_pkg:
                        del deps[old_pkg]
                    deps[new_name] = new_version

        with pkg_path.open("w", encoding="utf-8") as f:
            json.dump(pkg, f, ensure_ascii=False, indent=2)
            f.write("\n")

        print(f"[lynxtron] prepared module: {module}, "
              f"target_dir: {target_dir}, version: {args.version}")


def prepare_module(modules, packages_dir, cfg, args, env):
    for module in modules:
        module_cfg = cfg.get(module)
        if not module_cfg:
            raise RuntimeError(
                f"module not found in PREPARE_CONFIG: {module}")
        pkg_dir_cfg = module_cfg.get("package_dir")
        target_dir = (packages_dir / pkg_dir_cfg).resolve()

        # 1. prepare environment for module.
        prepare_script = module_cfg.get("prepare_script")
        if prepare_script:
            subprocess.run([str(x) for x in prepare_script],
                           cwd=str(target_dir), check=True, env=env)

        # 2. build module.
        build_script = module_cfg.get("build_script")
        if build_script:
            build_cmd = [str(x) for x in build_script]
            if module_cfg.get("build_pass_args", False):
                build_args = ["--version", args.version, "--tag", args.tag]
                if (len(build_cmd) >= 2
                        and build_cmd[0] == "npm"
                        and build_cmd[1] == "run"):
                    build_cmd = build_cmd + ["--"] + build_args
                else:
                    build_cmd = build_cmd + build_args
            print(f"[lynxtron] module: {module}, build_cmd: {build_cmd}, "
                  f"target_dir: {target_dir}")
            subprocess.run(build_cmd, cwd=str(target_dir), check=True, env=env)


def main():
    args = args_parser()
    script_path = Path(__file__).resolve()
    packages_dir = script_path.parent

    if args.module == "ALL":
        modules = list(PREPARE_CONFIG.keys())
    else:
        modules = [args.module]

    env = os.environ.copy()

    # 1. Update package.json (version + cross-package dep versions).
    module_init(modules, packages_dir, PREPARE_CONFIG, args)

    # 2. Run optional prepare / build scripts. No `npm publish` is performed
    #    here; the outer pipeline is expected to publish after this step.
    prepare_module(modules, packages_dir, PREPARE_CONFIG, args, env)


if __name__ == "__main__":
    main()
