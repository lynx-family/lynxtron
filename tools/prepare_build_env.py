# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import os
import platform
import sys

COLORED_YELLOW_MSG = '\033[33m'
COLORED_PRINT_END = '\033[0m'

def main():
    system = platform.system().lower()
    if system == "windows":
        hab = os.path.join(os.path.dirname(__file__), "hab.bat")
        envsetup = os.path.join(os.path.dirname(__file__), "envsetup.ps1")
        python3 = "python"
    else:
        hab = os.path.join(os.path.dirname(__file__), "hab")
        envsetup = os.path.join(os.path.dirname(__file__), "envsetup.sh")
        python3 = "python3"

    print(f"{COLORED_YELLOW_MSG}hab: {hab}{COLORED_PRINT_END}")
    print(f"{COLORED_YELLOW_MSG}envsetup: {envsetup}{COLORED_PRINT_END}")
    print(f"{COLORED_YELLOW_MSG}sync lynxtron dependencies............{COLORED_PRINT_END}")
    os.system(f"{hab} sync . -f --no-history --target lynxtron")
    print(f"{COLORED_YELLOW_MSG}sync tools dependencies............{COLORED_PRINT_END}")
    os.system(f"{hab} sync . -f --no-history --target tools --target-only")
    os.system(f"{hab} sync . -f --no-history --target tools_shared --target-only")
    print(f"{COLORED_YELLOW_MSG}sync lynx dependencies............{COLORED_PRINT_END}")
    os.system(f"{hab} sync . -f --no-history --target lynx --target-only")
    print(f"{COLORED_YELLOW_MSG}install lynxtron npm dependencies............{COLORED_PRINT_END}")
    os.system(f'{python3} script/lib/npx.py yarn@1.22.22 install --frozen-lockfile')
    print(f"{COLORED_YELLOW_MSG}setup build environment............{COLORED_PRINT_END}")
    os.system(f"{envsetup}")
    print(f"{COLORED_YELLOW_MSG}Build environment setup completed.{COLORED_PRINT_END}")


if __name__ == "__main__":
    sys.exit(main())
