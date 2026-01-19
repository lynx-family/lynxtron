#!/usr/bin/env python3
# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import subprocess

current_dir = os.path.dirname(os.path.realpath(__file__))

npm_command = ['npm', 'run', 'build']
subprocess.check_call(
        " ".join(npm_command),
        cwd=current_dir,
        shell=True,
    )