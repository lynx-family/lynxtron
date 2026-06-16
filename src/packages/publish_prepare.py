# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
#
# publish_prepare.py
#
# Prepare-only counterpart of //packages/publish.py for the publishing pipeline.
#
# Usage:
#   python3 publish_prepare.py --version 0.1.0
import argparse
import json
import os
import re
import subprocess
from pathlib import Path


# Files matching these suffixes inside `lynxtron-builder` / `lynxtron-rebuild`
# will be scanned and certain config entries rewritten to point at the open
# source release artifacts on GitHub.
SOURCE_REWRITE_EXTS = (
    ".js", ".ts", ".jsx", ".tsx", ".json",
    ".scss", ".css", ".html", ".mjs",
)
GITHUB_RELEASE_URL = "https://github.com/lynx-family/lynxtron/releases/download/"
MIRROR_REWRITE_PATTERN = re.compile(
    r"^(\s*)mirror:\s*['\"].*?['\"],?", flags=re.MULTILINE)
BASE_URL_REWRITE_PATTERN = re.compile(
    r"^(\s*)(export\s+)?const\s+BASE_URL\s*=\s*['\"].*?['\"]\s*;", flags=re.MULTILINE)


# Fields:
#   package_dir:     Path to the package, relative to this script's directory.
#   package_name:    Optional. If set, rewrite the module's own package.json
#                    `name` field to this value (e.g. rename `create-lynxtron`
#                    to `@lynx-js/create-lynxtron` for publishing).
#   replaces:        Optional. Map of dependency name -> { name, version }.
#                    Used to bump cross-package workspace dependencies (e.g.
#                    `workspace:*`) to the concrete version being published.
#                    If `version` is empty, args.version is used.
#                    The dependency name is NOT renamed (kept as @lynx-js/*).
#   prepare_script:  Optional. Command (list) executed before build.
#   build_script:    Optional. Command (list) executed to build the module.
#   build_pass_args: Optional. If true, append `--version` to build.
#   mirror_rewrite:  Optional. If true, rewrite `mirror: '...'` entries within
#                    the package's source files (excluding node_modules) to
#                    GITHUB_RELEASE_URL.
#   base_url_rewrite: Optional. If true, rewrite `const BASE_URL = '...';`
#                    entries within the package's source files (excluding
#                    node_modules) to GITHUB_RELEASE_URL.
PREPARE_CONFIG = {
    "lynxtron": {
        "package_dir": "./lynxtron",
        "base_url_rewrite": True,
    },
    # `npm run build` runs generate-template.js + generate-versions.js, which
    # copies the sibling `lynxtron-shell-demo` into `dist/lynxtron-shell-demo`,
    # writes `dist/versions.json`, and resolves `workspace:*` ranges in the
    # template's package.json files to concrete versions read from sibling
    # packages' package.json (which were just bumped by module_init above).
    "create-lynxtron": {
        "package_dir": "./create-lynxtron",
        "package_name": "@lynx-js/create-lynxtron",
        "build_script": ["npm", "run", "build"],
    },
    "create-browser-demo": {
        "package_dir": "./create-browser-demo",
        "build_script": ["npm", "run", "build"],
    },
    "lynxtron-builder": {
        "package_dir": "./lynxtron-builder",
        "mirror_rewrite": True,
    },
    "lynxtron-rebuild": {
        "package_dir": "./lynxtron-rebuild",
        "base_url_rewrite": True,
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
    parser.add_argument("--module", required=False, default="ALL",
                        help="Single module key to prepare. Defaults to ALL.")
    return parser.parse_args()


def rewrite_source_files(target_dir, patterns):
    """Rewrite simple single-line config entries in source files under
    target_dir, using a list of (compiled_pattern, replacement) tuples.
    Skips node_modules and only touches SOURCE_REWRITE_EXTS file types.
    """
    for root, _, files in os.walk(target_dir):
        if "node_modules" in root.split(os.sep):
            continue
        for file in files:
            if not file.endswith(SOURCE_REWRITE_EXTS):
                continue
            file_path = Path(root) / file
            try:
                content = file_path.read_text(encoding="utf-8")
                new_content = content
                for pattern, replacement in patterns:
                    new_content = pattern.sub(replacement, new_content)
                if new_content != content:
                    print(f"[lynxtron] rewrite source: {file_path}")
                    file_path.write_text(new_content, encoding="utf-8")
            except Exception as e:
                print(f"Error processing file {file_path}: {e}")


# @param modules: list of module names
def module_init(modules, packages_dir, cfg, args):
    for module in modules:
        module_cfg = cfg.get(module)
        if not module_cfg:
            raise RuntimeError(
                f"module not found in PREPARE_CONFIG: {module}")
        pkg_dir_cfg = module_cfg.get("package_dir")
        target_dir = (packages_dir / pkg_dir_cfg).resolve()

        # 1. Update version in package.json. Package name is kept untouched
        #    unless `package_name` is configured, in which case it is renamed.
        pkg_path = target_dir / "package.json"
        with pkg_path.open("r", encoding="utf-8") as f:
            pkg = json.load(f)
        pkg["version"] = args.version
        new_package_name = module_cfg.get("package_name")
        original_package_name = pkg["name"]
        if new_package_name and new_package_name != original_package_name:
            pkg["name"] = new_package_name

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

        # 3. Optionally rewrite single-line config entries in source files,
        #    e.g. download URLs, to point at GITHUB_RELEASE_URL.
        rewrite_patterns = []
        if module_cfg.get("mirror_rewrite", False):
            rewrite_patterns.append((
                MIRROR_REWRITE_PATTERN,
                r"\1mirror: '" + GITHUB_RELEASE_URL + "',",
            ))
        if module_cfg.get("base_url_rewrite", False):
            rewrite_patterns.append((
                BASE_URL_REWRITE_PATTERN,
                r"\1\2const BASE_URL = '" + GITHUB_RELEASE_URL + "';",
            ))
        if rewrite_patterns:
            rewrite_source_files(target_dir, rewrite_patterns)

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
                build_args = ["--version", args.version]
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
