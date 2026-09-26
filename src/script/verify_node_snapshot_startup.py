#!/usr/bin/env python3
# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

"""Smoke-test the browser process with the embedded Node startup snapshot."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--resources", required=True, type=Path)
    parser.add_argument("--arch", required=True)
    args = parser.parse_args()

    runtime = args.runtime.resolve(strict=True)
    resources = args.resources.resolve(strict=True)
    app_dir = resources / "app"
    app_dir.mkdir()
    try:
        (app_dir / "package.json").write_text(
            '{"name":"node-snapshot-smoke","version":"1.0.0","main":"index.js"}',
            encoding="utf-8",
        )
        (app_dir / "index.js").write_text(
            """const { app } = require('lynxtron');
const fs = require('fs');
fs.writeFileSync(process.env.LYNXTRON_SNAPSHOT_SMOKE_MARKER, JSON.stringify({
  snapshot: process.config.variables.node_use_node_snapshot,
  type: process.type,
  arch: process.arch,
  platform: process.platform,
}));
app.quit();
""",
            encoding="utf-8",
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            marker = Path(temp_dir) / "startup.json"
            env = os.environ.copy()
            env["LYNXTRON_SNAPSHOT_SMOKE_MARKER"] = str(marker)
            try:
                result = subprocess.run(
                    [str(runtime)],
                    cwd=runtime.parent,
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=30,
                    check=False,
                )
            except subprocess.TimeoutExpired as error:
                raise RuntimeError("Lynxtron startup timed out") from error
            if result.returncode != 0 or not marker.is_file():
                raise RuntimeError(
                    f"Lynxtron did not start the smoke app (exit {result.returncode})\n"
                    f"stdout: {result.stdout}\nstderr: {result.stderr}"
                )
            actual = json.loads(marker.read_text(encoding="utf-8"))
            expected_platform = "win32" if sys.platform == "win32" else sys.platform
            expected = {
                "snapshot": True,
                "type": "browser",
                "arch": args.arch,
                "platform": expected_platform,
            }
            if actual != expected:
                raise RuntimeError(f"Unexpected browser configuration: {actual!r}")
            print(f"Node snapshot browser smoke passed: {actual}")
    finally:
        shutil.rmtree(app_dir)


if __name__ == "__main__":
    main()
