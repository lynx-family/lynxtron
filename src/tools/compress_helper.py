# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import os
import sys
import zipfile

def compress_directory(target_dir):
    # Check if directory exists
    if not os.path.isdir(target_dir):
        print(f"Error: Directory '{target_dir}' does not exist.")
        sys.exit(1)

    # Normalize path, remove extra slashes
    target_dir = os.path.normpath(target_dir)
    
    # Get the archive name (default to same name as directory)
    base_name = os.path.basename(target_dir)
    if not base_name:
        base_name = "archive" # Fallback: if input is '.' or other special paths

    zip_filename = f"{base_name}.zip"
    print(f"Compressing directory '{target_dir}' to {zip_filename} ...")
    
    # Use zipfile module for precise control to fix issues where some unzip tools misidentify directories as files
    with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(target_dir):
            # Explicitly write directories (handle empty directories and ensure directory attributes)
            for d in dirs:
                dir_path = os.path.join(root, d)
                arcname = os.path.relpath(dir_path, target_dir)
                # Key: append '/' to path, strictly following zip specification for directory definition
                # This ensures all unzip tools (including Windows/Mac default tools) recognize it as a folder
                zipf.write(dir_path, arcname + '/')
                
            # Write regular files
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, target_dir)
                zipf.write(file_path, arcname)
    
    print(f"Compression complete! File generated: {os.path.abspath(zip_filename)}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python compress_helper.py <directory_relative_path>")
        sys.exit(1)
        
    target_directory = sys.argv[1]
    compress_directory(target_directory)
