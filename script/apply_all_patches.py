#!/usr/bin/env python3
# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import os
import warnings

from lib import git
from lib.patches import patch_from_dir

THREEWAY = "ELECTRON_USE_THREE_WAY_MERGE_FOR_PATCHES" in os.environ
COLORED_GREEN_MSG = '\033[92m'
COLORED_PRINT_END = '\033[0m'

def apply_patches(target):
  repo = target.get('repo')
  if not os.path.exists(repo):
    warnings.warn(f'repo not found: {repo}')
    return
  patch_dir = target.get('patch_dir')
  print(f'{COLORED_GREEN_MSG}applying patches from {patch_dir} to {repo}{COLORED_PRINT_END}')
  git.import_patches(
    committer_email="scripts@lynxtron",
    committer_name="Lynxtron Scripts",
    patch_data=patch_from_dir(patch_dir),
    repo=repo,
    threeway=THREEWAY,
  )

def apply_config(config):
  for target in config:
    apply_patches(target)

def parse_args():
  parser = argparse.ArgumentParser(description='Apply Lynxtron patches')
  parser.add_argument('config', nargs='+',
                      type=argparse.FileType('r'),
                      help='patches\' config(s) in the JSON format')
  return parser.parse_args()


def main():
  for config_json in parse_args().config:
    apply_config(json.load(config_json))


if __name__ == '__main__':
  main()
