# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import os
import platform
import sys
import time

COLORED_YELLOW_MSG = '\033[33m'
COLORED_RED_MSG = '\033[31m'
COLORED_GREEN_MSG = '\033[32m'

COLORED_PRINT_END = '\033[0m'

DEFAULT_HABITAT_CONCURRENCY = "2"
DEFAULT_HABITAT_SYNC_ATTEMPTS = 5
DEFAULT_HABITAT_RETRY_BASE_DELAY_SECONDS = 10

# Get the directory where the current script is located
current_dir = os.path.dirname(os.path.realpath(__file__))
# Get the root directory
root_dir = os.path.abspath(os.path.join(current_dir, '..'))
print(f"root_dir: {root_dir}")
sys.path.append(root_dir)
from src.script.abort_am_sessions import abort_am_sessions
from lynxtron_tools.habitat_lock import habitat_cache_lock
src_dir = os.path.join(root_dir, "src")
print(f"src_dir: {src_dir}")


def run_habitat_sync(command, description):
    """Run a Habitat sync with bounded exponential-backoff retries."""
    attempts = int(os.environ.get("HABITAT_SYNC_ATTEMPTS", DEFAULT_HABITAT_SYNC_ATTEMPTS))
    base_delay = int(
        os.environ.get(
            "HABITAT_RETRY_BASE_DELAY_SECONDS",
            DEFAULT_HABITAT_RETRY_BASE_DELAY_SECONDS,
        )
    )
    return_code = 0
    for attempt in range(1, attempts + 1):
        with habitat_cache_lock(description):
            return_code = os.system(command)
        if return_code == 0:
            return 0
        if attempt < attempts:
            delay = base_delay * (2 ** (attempt - 1))
            print(
                f"{COLORED_YELLOW_MSG}{description} failed "
                f"(attempt {attempt}/{attempts}, return_code={return_code}); "
                f"retrying in {delay} seconds...{COLORED_PRINT_END}"
            )
            time.sleep(delay)
    print(
        f"{COLORED_RED_MSG}{description} failed after "
        f"{attempts} attempts{COLORED_PRINT_END}"
    )
    return return_code


def configure_habitat_environment():
    os.environ["GIT_LFS_SKIP_SMUDGE"] = "1"
    os.environ.setdefault("HABITAT_CONCURRENCY", DEFAULT_HABITAT_CONCURRENCY)
    if os.environ.get("GITHUB_ACTIONS") == "true":
        # Keep Habitat and the cache action on the same cross-platform path.
        # Habitat falls back to the temp directory when HOME is unset on
        # Windows, while the cache action expands '~' through USERPROFILE.
        os.environ.setdefault(
            "HABITAT_CACHE_DIR", os.path.join(root_dir, ".habitat_cache")
        )
    print(
        f"{COLORED_YELLOW_MSG}Habitat concurrency: "
        f"{os.environ['HABITAT_CONCURRENCY']}{COLORED_PRINT_END}"
    )


def main():
    start_cwd = os.getcwd()
    system = platform.system().lower()
    if system == "windows":
        hab = os.path.join(os.path.dirname(__file__), "hab.ps1")
        envsetup = os.path.join(os.path.dirname(__file__), "envsetup.ps1")
        python3 = "python"
    else:
        hab = os.path.join(os.path.dirname(__file__), "hab")
        envsetup_file = os.path.join(os.path.dirname(__file__), "envsetup.sh")
        envsetup = f"source {envsetup_file}"
        python3 = "python3"

    configure_habitat_environment()
    habitat_cache_dir = os.path.realpath(
        os.path.expandvars(
            os.path.expanduser(os.environ.get("HABITAT_CACHE_DIR", "~/.habitat_cache"))
        )
    )
    habitat_cache_arg = f'--cache-dir "{habitat_cache_dir}"'
    print(f"{COLORED_YELLOW_MSG}hab: {hab}{COLORED_PRINT_END}")
    print(f"{COLORED_YELLOW_MSG}envsetup: {envsetup}{COLORED_PRINT_END}")
    print(f"{COLORED_GREEN_MSG}abort am sessions............{COLORED_PRINT_END}")
    abort_am_sessions()
    print(f"{COLORED_YELLOW_MSG}sync lynxtron dependencies............{COLORED_PRINT_END}")
    os.chdir(src_dir)
    if system == "windows":
        sync_lynxtron_cmd = f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history {habitat_cache_arg} --target lynxtron"
    else:
        sync_lynxtron_cmd = f"\"{hab}\" sync . -f --no-history {habitat_cache_arg} --target lynxtron"
    return_code = run_habitat_sync(sync_lynxtron_cmd, "sync lynxtron dependencies")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync lynxtron dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    print(f"{COLORED_YELLOW_MSG}sync tools dependencies............{COLORED_PRINT_END}")
    if system == "windows":
        sync_tools_cmd = f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history {habitat_cache_arg} --target tools --target-only"
    else:
        sync_tools_cmd = f"\"{hab}\" sync . -f --no-history {habitat_cache_arg} --target tools --target-only"
    return_code = run_habitat_sync(sync_tools_cmd, "sync tools dependencies")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync tools dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    if system == "windows":
        sync_tools_shared_cmd = f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history {habitat_cache_arg} --target tools_shared --target-only"
    else:
        sync_tools_shared_cmd = f"\"{hab}\" sync . -f --no-history {habitat_cache_arg} --target tools_shared --target-only"
    return_code = run_habitat_sync(sync_tools_shared_cmd, "sync tools_shared dependencies")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync tools_shared dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    print(f"{COLORED_YELLOW_MSG}sync lynx dependencies............{COLORED_PRINT_END}")
    if system == "windows":
        lynx_sync_cmd = f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history {habitat_cache_arg} --target lynx --target-only"
    else:
        lynx_sync_cmd = f"\"{hab}\" sync . -f --no-history {habitat_cache_arg} --target lynx --target-only"
    return_code = run_habitat_sync(lynx_sync_cmd, "sync lynx dependencies")
    if return_code != 0:
        print(f"{COLORED_RED_MSG}sync lynx dependencies failed, exit{COLORED_PRINT_END}")
        return return_code

    if system == "windows":
        skity_dir = os.path.join(root_dir, "third_party", "skity")
        original_skity_cwd = os.getcwd()
        try:
            os.chdir(skity_dir)
            print(f"{COLORED_YELLOW_MSG}sync skity dependencies............{COLORED_PRINT_END}")
            return_code = run_habitat_sync(
                f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history {habitat_cache_arg}",
                "sync skity dependencies",
            )
            if return_code != 0:
                print(f"{COLORED_RED_MSG}sync skity dependencies failed, exit{COLORED_PRINT_END}")
                return return_code
        finally:
            os.chdir(original_skity_cwd)

    print(f"{COLORED_YELLOW_MSG}install lynxtron npm dependencies............{COLORED_PRINT_END}")
    previous_skip_download = os.environ.get("LYNXTRON_SKIP_DOWNLOAD")
    os.environ["LYNXTRON_SKIP_DOWNLOAD"] = "1"
    try:
        return_code = os.system('node tools/yarn.js install --immutable')
    finally:
        if previous_skip_download is None:
            os.environ.pop("LYNXTRON_SKIP_DOWNLOAD", None)
        else:
            os.environ["LYNXTRON_SKIP_DOWNLOAD"] = previous_skip_download
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}install lynxtron npm dependencies failed, exit{COLORED_PRINT_END}")
        return return_code

    # apply lynx all patches
    original_dir = os.getcwd()
    try:
        os.chdir(root_dir)
        return_code = os.system(f"{python3} src/script/apply_all_patches.py src/patches/lynx/config.json")
        if return_code != 0:
            print(f"{COLORED_RED_MSG}apply_all_patches.py failed, exit{COLORED_PRINT_END}")
            return return_code
    finally:
        os.chdir(start_cwd)

    # apply skity patches
    original_dir = os.getcwd()
    try:
        os.chdir(root_dir)
        return_code = os.system(f"{python3} src/script/apply_all_patches.py src/patches/lynx/skity/config.json")
        if return_code != 0:
            print(f"{COLORED_RED_MSG}apply skity patches failed, exit{COLORED_PRINT_END}")
            return return_code
    finally:
        os.chdir(start_cwd)
   
    print(f"{COLORED_RED_MSG}Warning: One final step remains for the build environment, please run the following command manually:{COLORED_PRINT_END}")
    print(f"{COLORED_GREEN_MSG}{envsetup}{COLORED_PRINT_END}")
    return 0


if __name__ == "__main__":
    exit_code = main()
    if exit_code != 0:
        sys.exit(1)
    else:
        sys.exit(0)
