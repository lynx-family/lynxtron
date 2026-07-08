#!/usr/bin/env python3
# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import re
import shutil
import subprocess

current_dir = os.path.dirname(os.path.realpath(__file__))
src_dir = os.path.abspath(os.path.join(current_dir, "..", "..", ".."))


def node_major_version(node_path):
    try:
        version = subprocess.check_output(
            [node_path, "--version"], stderr=subprocess.STDOUT, text=True
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    match = re.match(r"v?(\d+)", version)
    return int(match.group(1)) if match else None


def find_node_command():
    candidates = []
    if os.name == "nt":
        candidates.extend(
            [
                r"C:\nodejs\node.exe",
                os.path.join(src_dir, "..", "buildtools", "node", "node.exe"),
            ]
        )
    candidates.append(
        os.path.join(
            src_dir,
            "..",
            "buildtools",
            "node",
            "bin",
            "node.exe" if os.name == "nt" else "node",
        )
    )
    path_node = shutil.which("node")
    if path_node:
        candidates.append(path_node)

    first_existing = None
    seen = set()
    for candidate in candidates:
        candidate = os.path.normpath(candidate)
        if candidate in seen or not os.path.exists(candidate):
            continue
        seen.add(candidate)
        first_existing = first_existing or candidate
        major = node_major_version(candidate)
        if major is not None and major >= 18:
            return candidate
    return first_existing or "node"


node_command = find_node_command()


def find_rspeedy_script():
    candidates = [
        os.path.join(
            current_dir, "node_modules", "@lynx-js", "rspeedy", "bin", "rspeedy.js"
        ),
        os.path.join(
            src_dir, "node_modules", "@lynx-js", "rspeedy", "bin", "rspeedy.js"
        ),
    ]
    return next((path for path in candidates if os.path.exists(path)), candidates[0])


rspeedy_script = find_rspeedy_script()
if not os.path.exists(rspeedy_script):
    yarn_env = os.environ.copy()
    yarn_env.setdefault("YARN_IGNORE_NODE", "1")
    subprocess.check_call(
        [
            node_command,
            os.path.join(src_dir, "tools", "yarn.js"),
            "install",
            "--immutable",
        ],
        cwd=src_dir,
        env=yarn_env,
    )
    rspeedy_script = find_rspeedy_script()

subprocess.check_call(
    [
        node_command,
        rspeedy_script,
        "build",
    ],
    cwd=current_dir,
)
