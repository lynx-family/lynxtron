#!/usr/bin/env python3
# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import os
import subprocess
import sys

source = sys.argv[1]
dest = sys.argv[2]

# Ensure any existing bundles is removed
subprocess.check_output(["rm", "-rf", dest])

subprocess.check_output(["cp", "-a", source, dest])