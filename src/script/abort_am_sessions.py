#!/usr/bin/env python3
# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import subprocess

def runCommand(cmd):
  p = subprocess.Popen(cmd,
                       shell=True,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT)
  return p.stdout.readlines()

# The git command returns bytes, so we decode it to a string
BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
print(f'BASE_DIR: {BASE_DIR}')

# Directories to check, relative to BASE_DIR
REPOS_TO_CHECK = [
    "base",
    "build",
    "third_party/node",
    "url",
    "tools",
    "v8",
    "lynx",
    "lynx/lynx/third_party/quickjs/src"
]

def abort_am_sessions():
    print(f"Starting check for incomplete 'git am' sessions from base directory: {BASE_DIR}")

    for repo_rel_path in REPOS_TO_CHECK:
        repo_abs_path = os.path.join(BASE_DIR, repo_rel_path)

        if not os.path.isdir(repo_abs_path):
            print(f"\nDirectory not found, skipping: {repo_abs_path}")
            continue

        # Check if it's a git repository
        git_dir = os.path.join(repo_abs_path, '.git')
        if not os.path.isdir(git_dir):
            print(f"\nNot a git repository, skipping: {repo_abs_path}")
            continue

        print(f"\nChecking status of: {repo_abs_path}")

        try:
            # Check git status
            status_process = subprocess.run(
                ['git', 'status'],
                cwd=repo_abs_path,
                capture_output=True,
                text=True,
                encoding='utf-8'
            )

            # The `git status` command might fail if in a weird state, but the output is what matters.
            output = status_process.stdout + status_process.stderr

            if "git am --abort" in output:
                print(f"  -> Found an incomplete 'git am' session. Aborting...")
                try:
                    # Abort the 'git am' session
                    abort_process = subprocess.run(
                        ['git', 'am', '--abort'],
                        cwd=repo_abs_path,
                        capture_output=True,
                        text=True,
                        encoding='utf-8',
                        check=True  # Throw an exception on failure
                    )
                    print(f"  -> Successfully aborted 'git am' in {repo_abs_path}")
                    if abort_process.stdout:
                        print(f"     {abort_process.stdout.strip()}")
                except subprocess.CalledProcessError as e:
                    print(f"  -> Failed to abort 'git am' in {repo_abs_path}")
                    print(f"     Error: {e.stderr.strip()}")
            else:
                print(f"  -> No 'git am' session in progress.")

        except FileNotFoundError:
            print(f"  -> 'git' command not found. Is Git installed and in your PATH?")
            # If git is not found, we can't proceed.
            return
        except Exception as e:
            print(f"  -> An unexpected error occurred while checking {repo_abs_path}: {e}")


def main():
    abort_am_sessions()

if __name__ == '__main__':
    main()
