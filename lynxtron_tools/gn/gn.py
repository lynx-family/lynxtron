#!/usr/bin/env python3
# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import argparse
import subprocess
import sys
import os
import json
import platform

CURRENT_PATH = os.path.dirname(__file__)
ROOT_PATH = os.path.dirname(os.path.dirname(CURRENT_PATH))

def get_current_os():
  system = platform.system()
  if system == 'Darwin':
    return 'mac'
  elif system == 'Windows':
    return 'win'
  else:
    return system.lower()

def get_default_gn_args(is_debug, enable_enlarge_stack):
  gn_args = ''
  if is_debug:
    gn_args += 'import("//src/build/args/debug.gn") '
  else:
    gn_args += 'import("//src/build/args/release.gn") '
  gn_args += 'desktop_enable_embedder_layer=true '
  gn_args += 'enable_clay_standalone=true '
  gn_args += 'disable_visibility_hidden=true '
  gn_args += 'use_ndk_static_cxx=false '
  gn_args += 'enable_linker_map=false '
  gn_args += 'enable_clay=true '
  gn_args += 'clay_enable_skshaper=true '
  gn_args += 'is_headless=true '
  gn_args += 'skia_enable_flutter_defines=true '
  gn_args += 'skia_use_dng_sdk=false '
  gn_args += 'skia_use_sfntly=false '
  gn_args += 'skia_enable_pdf=false '
  gn_args += 'skia_enable_svg=true '
  gn_args += 'enable_svg=true '
  gn_args += 'skia_enable_skottie=true '
  gn_args += 'skia_use_x11=false '
  gn_args += 'skia_use_wuffs=true '
  gn_args += 'skia_use_expat=true '
  gn_args += 'skia_use_fontconfig=false '
  gn_args += 'skia_use_icu=true '
  gn_args += 'allow_deprecated_api_calls=true '
  gn_args += 'stripped_symbols=true '
  gn_args += 'enable_lto=false '
  gn_args += 'enable_lepusng_worklet=true '
  gn_args += 'enable_napi_binding=true '
  gn_args += 'enable_inspector=true '
  gn_args += 'jsengine_type="v8" '
  gn_args += 'use_primjs_napi=true '
  gn_args += 'enable_skity=true '
  gn_args += 'textlayout_use_local_config=false '
  if enable_enlarge_stack:
    gn_args += 'enable_enlarge_stack=true '

  if get_current_os() == 'mac':
    gn_args += 'skia_gl_standard=""'
    gn_args += 'skia_use_metal=true '
    gn_args += 'shell_enable_metal=true '
    gn_args += 'use_clang_static_analyzer=false '
    gn_args += 'use_flutter_cxx=false '
  elif get_current_os() == 'win':
    gn_args += 'is_clang=true '

  return gn_args

def run_gn_script(gn_args, out_path):
  gn_script_dir = os.path.join(ROOT_PATH, 'buildtools', 'gn')
  gn_script_path = os.path.join(gn_script_dir, 'gn')
  if get_current_os() == 'win':
    gn_script_path = os.path.join(gn_script_dir, 'gn.exe')
  cmd = [gn_script_path, 'gen', out_path, f'--args={gn_args}']
  print(' '.join(cmd))
  subprocess.run(cmd, check=True)

def parse_args(args):
  args = args[1:]
  parser = argparse.ArgumentParser(description='A script run` gn gen`.')

  parser.add_argument('--gn-args', type=str, dest='gn_args')
  parser.add_argument('--is-debug', dest='is_debug', action='store_true', default=False)
  parser.add_argument('--mac-cpu', type=str, choices=['x64', 'arm64'], default='arm64')
  parser.add_argument('--windows-cpu', type=str, choices=['x64', 'arm64', 'x86'], default = 'x86')
  parser.add_argument('--enable-enlarge-stack', dest='enable_enlarge_stack', action='store_true', default=False)

  return parser.parse_args(args)

def main(argv):
  args = parse_args(argv)
  gn_args = ''
  gn_args += get_default_gn_args(args.is_debug, args.enable_enlarge_stack)
  if args.gn_args:
    gn_args += args.gn_args

  if get_current_os() == 'mac':
    gn_args += f' target_cpu="{args.mac_cpu}"'
  elif get_current_os() == 'win':
    gn_args += f' target_cpu="{args.windows_cpu}"'

  escaped_gn_args = gn_args.replace('"', '\\"')

  out_path = os.path.join(ROOT_PATH, 'out', 'Release')
  if args.is_debug:
    out_path = os.path.join(ROOT_PATH, 'out', 'Debug')
  return run_gn_script(gn_args, out_path)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
