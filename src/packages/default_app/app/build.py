#!/usr/bin/env python3
# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import shutil
import subprocess

current_dir = os.path.dirname(os.path.realpath(__file__))
src_dir = os.path.abspath(os.path.join(current_dir, "..", "..", ".."))
node_command = shutil.which("node") or os.path.join(
    src_dir,
    "..",
    "buildtools",
    "node",
    "bin",
    "node.exe" if os.name == "nt" else "node",
)


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
    subprocess.check_call(
        [
            node_command,
            os.path.join(src_dir, "tools", "yarn.js"),
            "install",
            "--immutable",
        ],
        cwd=src_dir,
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
