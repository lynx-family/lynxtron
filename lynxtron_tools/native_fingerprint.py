#!/usr/bin/env python3
"""Experimental local Ninja input fingerprint; NOT a cache-hit authorization.

Run after a successful build so compiler-discovered dependencies are available.
All recorded header dependencies are included conservatively, even for other
targets. Absolute paths are retained deliberately: cross-workspace reuse is not
yet supported. Environment/SDK inputs must be supplied explicitly.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def digest_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fingerprint(build_dir, target, tools, identity, ninja="ninja"):
    build_dir = Path(build_dir).resolve()
    if not (build_dir / ".ninja_deps").is_file():
        raise ValueError("Build first: compiler dependency log is missing")

    def query(*args):
        return subprocess.check_output(
            [ninja, "-t", *args], cwd=build_dir, text=True)

    # Ninja shell-quotes input paths; this initial experiment is POSIX-only.
    inputs = set(shlex.split(query("inputs", target)))
    generated = {str((build_dir / line.rsplit(": ", 1)[0]).resolve())
                 for line in query("targets", "all").splitlines()}
    dependencies = query("deps")
    if not dependencies.strip() or "(STALE)" in dependencies:
        raise ValueError("Build first: dependency log is empty or stale")
    for line in dependencies.splitlines():
        if line.startswith("    "):
            inputs.add(line[4:])

    files = {}
    for value in sorted(inputs):
        path = Path(value)
        if not path.is_absolute():
            path = build_dir / path
        if str(path.resolve()) in generated:
            # Hash producer inputs/commands, not outputs requiring compilation.
            continue
        if path.is_dir():
            # CMake emits directory/order-only dependencies. Do not recursively
            # hash the build directory (it contains outputs and mutable logs).
            files[str(path)] = {"directory": str(path.resolve())}
            continue
        # Preserve the logical path as well as the resolved target identity.
        files[str(path)] = {
            "resolved": str(path.resolve(strict=True)),
            "sha256": digest_file(path),
        }
    tool_files = {str(Path(p).resolve(strict=True)): digest_file(Path(p))
                  for p in tools}
    if not tool_files or not identity:
        raise ValueError("Explicit tool files and environment/SDK identity required")
    manifest = {
        "schema": 1,
        "scope": "local-experiment-not-cache-authorization",
        "target": target,
        "commands": query("commands", target),
        "files": files,
        "tools": tool_files,
        "identity": identity,
    }
    data = json.dumps(manifest, sort_keys=True, separators=(",", ":"))
    return {"fingerprint": hashlib.sha256(data.encode()).hexdigest(),
            "manifest": manifest}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--tool", action="append", required=True)
    parser.add_argument("--identity", required=True,
                        help="Explicit SDK/environment identity; not inferred")
    parser.add_argument("--ninja", default="ninja")
    args = parser.parse_args()
    print(json.dumps(fingerprint(args.build_dir, args.target, args.tool,
                                 args.identity, args.ninja), indent=2))


if __name__ == "__main__":
    main()
