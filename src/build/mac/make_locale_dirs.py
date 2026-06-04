#!/usr/bin/env python3

"""Creates macOS .lproj directories used by localized app bundles."""

import errno
import os
import sys


def main(args):
  for dirname in args:
    try:
      os.makedirs(dirname)
    except OSError as error:
      if error.errno != errno.EEXIST:
        raise


if __name__ == "__main__":
  main(sys.argv[1:])
