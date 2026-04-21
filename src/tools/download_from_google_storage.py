#!/usr/bin/env python3
# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
"""Simple script to download files from Google Storage without gsutil.
Usage: download_from_google_storage.py --bucket <bucket> -s <sha1_file>
Example: download_from_google_storage.py --bucket chromium-browser-clang/rc -s ../build/toolchain/win/rc/win/rc.exe.sha1
"""

import argparse
import hashlib
import os
import sys
import urllib.request


def get_sha1(filename):
    """Calculate SHA1 of a file."""
    sha1 = hashlib.sha1()
    with open(filename, 'rb') as f:
        for chunk in iter(lambda: f.read(8192), b''):
            sha1.update(chunk)
    return sha1.hexdigest()


def main():
    parser = argparse.ArgumentParser(description='Download file from Google Storage')
    parser.add_argument('--bucket', required=True, help='Google Storage bucket name')
    parser.add_argument('-s', '--sha1_file', required=True, help='Path to .sha1 file')
    args = parser.parse_args()
    
    bucket = args.bucket
    sha1_file = os.path.abspath(args.sha1_file)
    
    if not os.path.exists(sha1_file):
        print(f'ERROR: sha1_file does not exist: {sha1_file}')
        return 1
    
    # Output file is in same directory as sha1_file, with .sha1 removed
    if sha1_file.endswith('.sha1'):
        output_file = sha1_file[:-5]
    else:
        output_file = sha1_file
    
    with open(sha1_file, 'r') as f:
        expected_sha1 = f.read().strip()
    
    # Check if we already have the file with correct SHA1
    if os.path.exists(output_file):
        if get_sha1(output_file) == expected_sha1:
            print(f'{output_file} already exists and matches SHA1.')
            return 0
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    # Google Storage public URL
    url = f'https://storage.googleapis.com/{bucket}/{expected_sha1}'
    print(f'Downloading {url} to {output_file}...')
    
    try:
        urllib.request.urlretrieve(url, output_file)
        print(f'Download complete.')
        
        # Verify
        downloaded_sha1 = get_sha1(output_file)
        if downloaded_sha1 == expected_sha1:
            print(f'SHA1 matches: {downloaded_sha1}')
            return 0
        else:
            print(f'ERROR: SHA1 mismatch! Expected {expected_sha1}, got {downloaded_sha1}')
            os.remove(output_file)
            return 1
    except Exception as e:
        print(f'ERROR: Download failed: {e}')
        if os.path.exists(output_file):
            os.remove(output_file)
        return 1


if __name__ == '__main__':
    sys.exit(main())
