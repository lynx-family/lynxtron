#!/usr/bin/env python3
# Copyright 2024 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import argparse
import os.path
import re
import platform

system = platform.system()

def generate_gn_args_file(file, properties):
    content = '# Generated from dependencies/DEPS\n'

    dir_path = os.path.dirname(file)
    
    if dir_path:
        os.makedirs(dir_path, exist_ok=True)

    for k, v in properties.items():
        content += f'{k} = {v}\n'

    with open(file, 'w') as f:
        f.write(content)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-f', '--file', nargs='+', help="path to gn args file")
    parser.add_argument(
        '-p', '--properties', nargs='+',
        help='property list to be updated, which has a format of "name1=value1 name2=value2"'
    )

    args = parser.parse_args()
    file_list = args.file if isinstance(args.file, list) else [args.file]
    property_list = args.properties if isinstance(args.properties, list) else [args.properties]
    properties = {}
    for property in property_list:
        exp_list = property.split("=")
        key = exp_list[0]
        value = exp_list[1]
        properties[key] = value
    for file in file_list:
        generate_gn_args_file(file, properties)

if __name__ == '__main__':
    main()
