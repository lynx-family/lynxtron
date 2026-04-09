# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import os
import platform
import sys

COLORED_YELLOW_MSG = '\033[33m'
COLORED_RED_MSG = '\033[31m'
COLORED_GREEN_MSG = '\033[32m'

COLORED_PRINT_END = '\033[0m'

# Get the directory where the current script is located
current_dir = os.path.dirname(os.path.realpath(__file__))
# Get the root directory
root_dir = os.path.abspath(os.path.join(current_dir, '..'))
print(f"root_dir: {root_dir}")
sys.path.append(root_dir)
from script.abort_am_sessions import abort_am_sessions

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

    os.environ["GIT_LFS_SKIP_SMUDGE"] = "1"
    print(f"{COLORED_YELLOW_MSG}hab: {hab}{COLORED_PRINT_END}")
    print(f"{COLORED_YELLOW_MSG}envsetup: {envsetup}{COLORED_PRINT_END}")
    print(f"{COLORED_GREEN_MSG}abort am sessions............{COLORED_PRINT_END}")
    abort_am_sessions()
    print(f"{COLORED_YELLOW_MSG}sync lynxtron dependencies............{COLORED_PRINT_END}")
    os.chdir(root_dir)
    if system == "windows":
        return_code = os.system(f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history --target lynxtron")
    else:
        return_code = os.system(f"\"{hab}\" sync . -f --no-history --target lynxtron")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync lynxtron dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    print(f"{COLORED_YELLOW_MSG}sync tools dependencies............{COLORED_PRINT_END}")
    if system == "windows":
        return_code = os.system(f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history --target tools --target-only")
    else:
        return_code = os.system(f"\"{hab}\" sync . -f --no-history --target tools --target-only")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync tools dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    if system == "windows":
        return_code = os.system(f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history --target tools_shared --target-only")
    else:
        return_code = os.system(f"\"{hab}\" sync . -f --no-history --target tools_shared --target-only")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync tools_shared dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    print(f"{COLORED_YELLOW_MSG}sync lynx dependencies............{COLORED_PRINT_END}")
    if system == "windows":
        return_code = os.system(f"powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \"{hab}\" sync . -f --no-history --target lynx --target-only")
    else:
        return_code = os.system(f"\"{hab}\" sync . -f --no-history --target lynx --target-only")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync lynx dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    
    print(f"{COLORED_YELLOW_MSG}install lynxtron npm dependencies............{COLORED_PRINT_END}")
    return_code = os.system(f'node tools/yarn.js install --immutable')
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}install lynxtron npm dependencies failed, exit{COLORED_PRINT_END}")
        return return_code

    # apply lynx all patches
    original_dir = os.getcwd()
    try:
        os.chdir("..")
        return_code = os.system(f"{python3} src/script/apply_all_patches.py src/patches/lynx/config.json")
        if return_code != 0:
            print(f"{COLORED_RED_MSG}apply_all_patches.py failed, exit{COLORED_PRINT_END}")
            return return_code
    finally:
        os.chdir(start_cwd)
   
    print(f"{COLORED_RED_MSG}Warning: One final step remains for the build environment, please run the following command manually:{COLORED_PRINT_END}")
    print(f"{COLORED_GREEN_MSG}{envsetup}{COLORED_PRINT_END}")


if __name__ == "__main__":
    sys.exit(main())
