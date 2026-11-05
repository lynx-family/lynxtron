"""Real local arm64 Lynxtron build/cache round trip, never a CI shortcut.

The first run always compiles. Cache restoration uses a fresh destination and
does not replace the build output. Full input trees protect absent-header cases.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time

from local_native_cache import LocalCache, file_hash, input_key


def run(root, component="lynxtron"):
    if platform.system() != "Darwin":
        raise RuntimeError("This acceptance runner is macOS-only")
    root = Path(root).resolve()
    if not (root / ".native-cache-acceptance-checkout").is_file():
        raise RuntimeError("Not an isolated acceptance checkout")
    if shutil.disk_usage(root).free < 25 * 1024**3:
        raise RuntimeError("Less than 25 GiB free; refusing full native build")

    def command(*args):
        subprocess.run(args, cwd=root, check=True)

    excluded = [root / "out"]
    if component == "cef-webview":
        entry = root / "lynxtron_tools/build_cef_webview.py"
        if not entry.is_file():
            raise RuntimeError("CEF acceptance requires the #258 build entry")
        command(sys.executable, str(entry), "--arch", "arm64")
        package = root / "src/packages/cef-webview"
        app = package / "dist/darwin/arm64"
        excluded.extend([package / "dist", package / "build"])
    else:
        command(sys.executable, "lynxtron_tools/gn/gn.py", "--mac-cpu", "arm64")
        app = root / "out/Release/Lynxtron.app"
    sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    # Deliberately broad for the first real acceptance: no source-package
    # exclusions, inferred external toolchain equivalence, or path rewriting.
    roots = [p for p in root.iterdir() if p.name not in {
        ".git", ".venv", "out", ".native-cache-acceptance-checkout"}]
    roots.append(Path(sdk))
    roots.append(Path(subprocess.check_output(
        ["xcrun", "--find", "clang++"], text=True).strip()))
    identity = {"component": component, "arch": "arm64", "variant": "release", "sdk": sdk,
                "xcode": subprocess.check_output(["xcodebuild", "-version"],
                                                  text=True),
                "environment_digest_only": dict(os.environ)}
    last_progress = [0.0]
    def progress(path):
        if time.monotonic() - last_progress[0] > 10:
            print(f"Fingerprint scanning: {path}", flush=True)
            last_progress[0] = time.monotonic()
    key = input_key(roots, identity, excluded, progress)
    if component == "lynxtron":
        command("ninja", "-C", "out/Release", "-j", "4", "lynxtron_app")
    if not app.is_dir():
        raise RuntimeError(f"Expected complete native artifact: {app}")
    if input_key(roots, identity, excluded) != key:
        raise RuntimeError("Inputs changed during build; refusing cache admission")
    cache = LocalCache(Path.home() / ".habitat_cache/native-artifacts-v1",
                       2 * 1024**3, 2)
    stored = cache.put(key, app)
    if not stored:
        raise RuntimeError("App exceeds bounded cache budget; not cached")
    # Compare every file and symlink of the actual bundle, not just its main exe.
    def inventory(directory):
        result = {}
        for path in directory.rglob("*"):
            name = str(path.relative_to(directory))
            if path.is_symlink():
                result[name] = {"link": os.readlink(path)}
            elif path.is_file():
                result[name] = {"sha256": file_hash(path),
                                "executable": bool(path.stat().st_mode & 0o111)}
        return result

    with tempfile.TemporaryDirectory(prefix="lynxtron-cache-restore-") as tmp:
        restored = Path(tmp) / "payload"
        if not cache.restore(key, restored):
            raise RuntimeError("Real app cache restore failed")
        if inventory(app) != inventory(restored):
            raise RuntimeError("Restored app differs from source build")
        if component == "cef-webview":
            executables = [restored / "cef_extension.node"]
            executables.extend(restored.glob("**/*.app/Contents/MacOS/*"))
            framework = (restored / "frameworks/Chromium Embedded Framework.framework"
                         / "Chromium Embedded Framework")
            executables.append(framework)
            if len(executables) < 3:
                raise RuntimeError("CEF helper apps missing from restored payload")
        else:
            executables = [restored / "Contents/MacOS/Lynxtron"]
        for binary in executables:
            command("lipo", "-verify_arch", "arm64", str(binary))
    print(json.dumps({"real_app_cache_roundtrip": "pass", "component": component, "key": key,
                      "max_bytes": cache.max_bytes, "max_entries": 2}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root")
    parser.add_argument("--component", choices=["lynxtron", "cef-webview"], default="lynxtron")
    options = parser.parse_args()
    run(options.root, options.component)
