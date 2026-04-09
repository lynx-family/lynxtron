# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import subprocess
import sys


def npm(*npm_args):
    call_args = [__get_executable_name()] + list(npm_args)
    subprocess.check_call(call_args)


def __get_executable_name():
    executable = 'npm'
    if sys.platform == 'win32':
        executable += '.cmd'
    return executable


if __name__ == '__main__':
    npm(*sys.argv[1:])
