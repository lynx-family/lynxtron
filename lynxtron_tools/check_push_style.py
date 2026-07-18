#!/usr/bin/env python3
# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request


PRETTIER_EXTENSIONS = {
    ".js",
    ".jsx",
    ".mjs",
    ".cjs",
    ".ts",
    ".tsx",
    ".yml",
    ".yaml",
}
RSLINT_EXTENSIONS = {".js", ".jsx", ".mjs", ".cjs", ".ts", ".tsx"}
CLANG_FORMAT_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hpp",
    ".java",
    ".m",
    ".mm",
}
GN_EXTENSIONS = {".gn", ".gni"}
FORMATTED_EXTENSIONS = (
    PRETTIER_EXTENSIONS | CLANG_FORMAT_EXTENSIONS | GN_EXTENSIONS
)
PINNED_FORMAT_TOOLS = {
    "clang-format": {
        "release": "clang-format-020d2fb7",
        "archive": "buildtools-clang-format",
        "version": "020d2fb",
        "systems": {"darwin", "linux"},
    },
    "gn": {
        "release": "gn-cc28efe6",
        "archive": "buildtools-gn",
        "version": "cc28efe62ef0",
        "systems": {"darwin", "linux", "windows"},
    },
}


def run(command: list[str], cwd: Path, *, capture_output: bool = False) -> None:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        text=True,
        capture_output=capture_output,
    )
    if capture_output and result.stdout:
        print(result.stdout, end="")
    if capture_output and result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode:
        raise subprocess.CalledProcessError(result.returncode, command)


def changed_files(root: Path, base: str, head: str) -> list[Path]:
    output = subprocess.check_output(
        [
            "git",
            "diff",
            "--name-only",
            "--diff-filter=ACMR",
            "-z",
            base,
            head,
        ],
        cwd=root,
    )
    return [
        root / path.decode()
        for path in output.split(b"\0")
        if path and (root / path.decode()).is_file()
    ]


def staged_files(root: Path) -> list[Path]:
    output = subprocess.check_output(
        [
            "git",
            "diff",
            "--cached",
            "--name-only",
            "--diff-filter=ACMR",
            "-z",
        ],
        cwd=root,
    )
    return [
        root / path.decode()
        for path in output.split(b"\0")
        if path and (root / path.decode()).is_file()
    ]


def ensure_pushed_content_is_checked(root: Path, head: str, files: list[Path]) -> None:
    if not files:
        return
    relative_files = [os.fspath(path.relative_to(root)) for path in files]
    output = subprocess.check_output(
        ["git", "diff", "--name-only", "-z", head, "--", *relative_files],
        cwd=root,
    )
    modified = [path.decode() for path in output.split(b"\0") if path]
    if modified:
        print(
            "Commit or stash local changes to files included in this push:",
            file=sys.stderr,
        )
        for path in modified:
            print(f"  {path}", file=sys.stderr)
        raise RuntimeError("pushed files differ from the working tree")


def ensure_staged_content_is_checked(root: Path, files: list[Path]) -> None:
    if not files:
        return
    relative_files = [os.fspath(path.relative_to(root)) for path in files]
    output = subprocess.check_output(
        ["git", "diff", "--name-only", "-z", "--", *relative_files],
        cwd=root,
    )
    modified = [path.decode() for path in output.split(b"\0") if path]
    if modified:
        print(
            "The pre-commit formatter cannot safely update partially staged files:",
            file=sys.stderr,
        )
        for path in modified:
            print(f"  {path}", file=sys.stderr)
        raise RuntimeError("stage or stash these files before committing")


def ensure_js_tools(style_tools: Path) -> tuple[Path, Path]:
    executable_suffix = ".cmd" if platform.system().lower() == "windows" else ""
    prettier = (
        style_tools / "node_modules" / ".bin" / f"prettier{executable_suffix}"
    )
    rslint = style_tools / "node_modules" / ".bin" / f"rslint{executable_suffix}"
    if prettier.is_file() and rslint.is_file():
        return prettier, rslint

    if not shutil.which("npm"):
        raise RuntimeError("Node.js with npm is required to install style tools.")

    print("Installing the JavaScript style tools...")
    run(
        ["npm", "ci", "--ignore-scripts", "--no-audit", "--no-fund"],
        style_tools,
    )
    return prettier, rslint


def tool_platform() -> tuple[str, str]:
    system = platform.system().lower()
    machine = platform.machine().lower()
    machine = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
    return system, machine


def tool_has_expected_version(tool: Path, marker: str) -> bool:
    if not tool.is_file():
        return False
    try:
        version = subprocess.check_output(
            [tool, "--version"], text=True, stderr=subprocess.STDOUT
        )
    except (OSError, subprocess.CalledProcessError):
        return False
    return marker in version


def extract_tool(archive: Path, destination: Path, binary_name: str) -> Path:
    with tempfile.TemporaryDirectory() as temporary_directory:
        extract_root = Path(temporary_directory).resolve()
        with tarfile.open(archive, "r:gz") as tar:
            for member in tar.getmembers():
                member_path = (extract_root / member.name).resolve()
                if (
                    not member_path.is_relative_to(extract_root)
                    or member.issym()
                    or member.islnk()
                ):
                    raise RuntimeError(f"unsafe path in formatter archive: {member.name}")
            tar.extractall(extract_root)

        matches = list(extract_root.rglob(binary_name))
        if len(matches) != 1:
            raise RuntimeError(f"could not find {binary_name} in {archive}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(matches[0], destination)
        destination.chmod(0o755)
    return destination


def ensure_format_tool(root: Path, name: str) -> Path:
    metadata = PINNED_FORMAT_TOOLS[name]
    system, machine = tool_platform()
    if system not in metadata["systems"]:
        raise RuntimeError(f"the pinned {name} binary is unavailable on {system}")

    executable_name = f"{name}.exe" if system == "windows" else name
    repository_candidates = {
        "clang-format": [
            root / "src" / "tools_shared" / "buildtools" / "clang-format" / name
        ],
        "gn": [
            root / "buildtools" / "gn" / executable_name,
            root / "src" / "tools_shared" / "buildtools" / "gn" / executable_name,
        ],
    }
    path_candidate = shutil.which(name)
    candidates = repository_candidates[name]
    if path_candidate:
        candidates.append(Path(path_candidate))
    for candidate in candidates:
        if tool_has_expected_version(candidate, metadata["version"]):
            return candidate

    cache_root = (
        Path.home()
        / ".cache"
        / "lynxtron"
        / "format-tools"
        / metadata["release"]
        / f"{system}-{machine}"
    )
    cached_binary = cache_root / executable_name
    if tool_has_expected_version(cached_binary, metadata["version"]):
        return cached_binary

    archive_name = f"{metadata['archive']}-{system}-{machine}.tar.gz"
    url = (
        "https://github.com/lynx-family/buildtools/releases/download/"
        f"{metadata['release']}/{archive_name}"
    )
    cache_root.mkdir(parents=True, exist_ok=True)
    archive = cache_root / archive_name
    print(f"Downloading pinned {name} from {url}...")
    urllib.request.urlretrieve(url, archive)
    extract_tool(archive, cached_binary, executable_name)
    archive.unlink()
    if not tool_has_expected_version(cached_binary, metadata["version"]):
        raise RuntimeError(f"downloaded {name} has an unexpected version")
    return cached_binary


def check_newline(files: list[Path]) -> None:
    missing = [path for path in files if path.read_bytes()[-1:] != b"\n"]
    if missing:
        print("The following files must end with a newline:", file=sys.stderr)
        for path in missing:
            print(f"  {path}", file=sys.stderr)
        raise RuntimeError("missing trailing newline")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check or write lint and formatting for changed files."
    )
    parser.add_argument("--base")
    parser.add_argument("--head")
    parser.add_argument("--staged", action="store_true")
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--github", action="store_true")
    args = parser.parse_args()

    if args.staged:
        if args.base or args.head:
            parser.error("--staged cannot be combined with --base or --head")
    elif not args.base or not args.head:
        parser.error("--base and --head are required unless --staged is used")
    if args.write and not args.staged:
        parser.error("--write requires --staged")

    root = Path(
        subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"], text=True
        ).strip()
    )
    src = root / "src"
    style_tools = root / "lynxtron_tools" / "style"
    if args.staged:
        files = staged_files(root)
        ensure_staged_content_is_checked(root, files)
    else:
        files = changed_files(root, args.base, args.head)
        ensure_pushed_content_is_checked(root, args.head, files)
    formatted_files = [path for path in files if path.suffix in FORMATTED_EXTENSIONS]

    if formatted_files and not args.write:
        check_newline(formatted_files)

    prettier_files = [path for path in files if path.suffix in PRETTIER_EXTENSIONS]
    rslint_files = [
        path
        for path in files
        if path.suffix in RSLINT_EXTENSIONS and path.is_relative_to(src)
    ]
    if prettier_files or rslint_files:
        prettier, rslint = ensure_js_tools(style_tools)

    if prettier_files:
        run(
            [
                prettier,
                "--write" if args.write else "--check",
                *[os.fspath(path) for path in prettier_files],
            ],
            src,
        )

    clang_files = [path for path in files if path.suffix in CLANG_FORMAT_EXTENSIONS]
    if clang_files:
        clang_format = ensure_format_tool(root, "clang-format")
        for path in clang_files:
            if args.write:
                run([clang_format, "-i", path], root)
            else:
                formatted = subprocess.check_output(
                    [clang_format, os.fspath(path)], cwd=root
                )
                if formatted != path.read_bytes():
                    raise RuntimeError(f"clang-format check failed for {path}")

    gn_files = [path for path in files if path.suffix in GN_EXTENSIONS]
    if gn_files:
        gn = ensure_format_tool(root, "gn")
        for path in gn_files:
            command = [gn, "format"]
            if not args.write:
                command.append("--dry-run")
            command.append(os.fspath(path))
            result = subprocess.run(
                command,
                cwd=root,
                check=False,
                capture_output=True,
                text=True,
            )
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="", file=sys.stderr)
            if result.returncode or (
                not args.write and (result.stdout or result.stderr)
            ):
                raise RuntimeError(f"GN formatting failed for {path}")

    if args.write and formatted_files:
        check_newline(formatted_files)
        run(
            [
                "git",
                "add",
                "--",
                *[os.fspath(path.relative_to(root)) for path in formatted_files],
            ],
            root,
        )
        print(f"Formatted and staged {len(formatted_files)} file(s).")

    if rslint_files:
        command = [rslint, "--config", style_tools / "rslint.config.mjs"]
        if args.github:
            command.extend(["--format", "github"])
        command.extend(os.fspath(path.relative_to(src)) for path in rslint_files)
        run(command, src)

    print(f"Rslint and formatting checks passed for {len(files)} changed file(s).")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"style check failed: {error}", file=sys.stderr)
        sys.exit(1)
