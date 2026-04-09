# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import argparse
import os
import subprocess


def decompress_file(file_path: str, dest_dir: str):
    # Ensure the destination directory exists
    os.makedirs(dest_dir, exist_ok=True)

    _, file_extension = os.path.splitext(file_path)

    if file_extension == '.gz' and '.tar' in file_path:
        file_extension = '.tar.gz'
    elif file_extension == '.xz' and '.tar' in file_path:
        file_extension = '.tar.xz'
    elif file_extension == '.tgz':
        file_extension = '.tar.gz'

    file_extension = file_extension[1:]  # remove the dot

    commands = {
        'zip': ['unzip', file_path, '-d', dest_dir],
        'tar': ['tar', '-xf', file_path, '-C', dest_dir],
        'gz': [
            'gunzip', '-k', file_path, '-c', '>',
            os.path.join(dest_dir,
                         os.path.basename(file_path.replace('.gz', '')))
        ],
        'tar.gz': ['tar', '-zxf', file_path, '-C', dest_dir],
        'bz2': [
            'bunzip2', '-k', file_path, '-c', '>',
            os.path.join(dest_dir,
                         os.path.basename(file_path.replace('.bz2', '')))
        ],
        'tar.bz2': ['tar', '-jxf', file_path, '-C', dest_dir],
        'xz': [
            'unxz', '-k', file_path, '-c', '>',
            os.path.join(dest_dir,
                         os.path.basename(file_path.replace('.xz', '')))
        ],
        'tar.xz': ['tar', '-Jxf', file_path, '-C', dest_dir]
    }

    # Get the appropriate command by file extension
    command = commands.get(file_extension)

    if command:
        if file_extension in ['gz', 'bz2', 'xz']:
            subprocess.run(' '.join(command), shell=True)
        else:
            subprocess.run(command, shell=False)
    else:
        print(f"Unknown file extension: {file_extension}")


if __name__ == "__main__":
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Extract file to specified directory (append mode, no overwrites)')
    parser.add_argument('--file_path', required=True, help='Path to the file to be decompressed')
    parser.add_argument('--output_dir', required=True, help='Target directory for extraction (files will be appended here)')

    args = parser.parse_args()

    # Validate file existence
    if not os.path.isfile(args.file_path):
        print(f"Error: File not found - {args.file_path}")
        exit(1)

    # Execute extraction
    decompress_file(args.file_path, args.output_dir)
    print("Extraction completed")
