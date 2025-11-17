#!/usr/bin/env python3
import os
import subprocess
import sys

source = sys.argv[1]
dest = sys.argv[2]

# Ensure any existing bundles is removed
subprocess.check_output(["rm", "-rf", dest])

subprocess.check_output(["cp", "-a", source, dest])