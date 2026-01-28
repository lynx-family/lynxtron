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

def main():
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
    print(f"{COLORED_YELLOW_MSG}sync lynxtron dependencies............{COLORED_PRINT_END}")
    return_code = os.system(f"{hab} sync . -f --no-history --target lynxtron")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync lynxtron dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    print(f"{COLORED_YELLOW_MSG}sync tools dependencies............{COLORED_PRINT_END}")
    return_code = os.system(f"{hab} sync . -f --no-history --target tools --target-only")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync tools dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    return_code = os.system(f"{hab} sync . -f --no-history --target tools_shared --target-only")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync tools_shared dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    print(f"{COLORED_YELLOW_MSG}sync lynx dependencies............{COLORED_PRINT_END}")
    return_code = os.system(f"{hab} sync . -f --no-history --target lynx --target-only")
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}sync lynx dependencies failed, exit{COLORED_PRINT_END}")
        return return_code
    print(f"{COLORED_YELLOW_MSG}install lynxtron npm dependencies............{COLORED_PRINT_END}")
    return_code = os.system(f'{python3} script/lib/npx.py yarn install --immutable')
    if return_code != 0:
        print(f"{COLORED_YELLOW_MSG}install lynxtron npm dependencies failed, exit{COLORED_PRINT_END}")
        return return_code

    # apply lynx all patches
    original_dir = os.getcwd()
    try:
        os.chdir("..")
        return_code = os.system(f"{python3} lynxtron/script/apply_all_patches.py lynxtron/patches/lynx/config.json")
        if return_code != 0:
            print(f"{COLORED_RED_MSG}apply_all_patches.py failed, exit{COLORED_PRINT_END}")
            return return_code
    finally:
        os.chdir(original_dir)
   
    print(f"{COLORED_RED_MSG}Warning: One final step remains for the build environment, please run the following command manually:{COLORED_PRINT_END}")
    print(f"{COLORED_GREEN_MSG}{envsetup}{COLORED_PRINT_END}")


if __name__ == "__main__":
    sys.exit(main())
