#!/usr/bin/env python3
"""Prepare macOS host tools and target dependencies using the shared setup."""
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def target_environment(arch):
    if arch not in ('arm64', 'x64'):
        raise ValueError(f'Unsupported macOS target: {arch}')
    env = dict(os.environ)
    env.update(npm_config_arch=arch, npm_config_platform='darwin',
               LYNXTRON_SKIP_DOWNLOAD='1')
    # Habitat selects Node for the host. Only DEPS.extension consumes the
    # requested architecture when selecting the CEF SDK.
    tool_paths = [ROOT / '.venv/bin', ROOT / 'buildtools/node/bin',
                  ROOT / 'buildtools/cmake/CMake.app/Contents/bin']
    env['PATH'] = os.pathsep.join(map(str, tool_paths)) + os.pathsep + env['PATH']
    return env


def prepare_python_environment(env):
    subprocess.run([sys.executable, str(ROOT / 'lynxtron_tools/vpython_tools/vpython_env_setup.py'),
                    '--root_dir', str(ROOT)], cwd=ROOT, env=env, check=True)
    env['VIRTUAL_ENV'] = str(ROOT / '.venv')
    env.pop('PYTHONHOME', None)
    return str(ROOT / '.venv/bin/python3')


def prepare(arch):
    if platform.system() != 'Darwin':
        raise RuntimeError('macOS and Xcode Command Line Tools are required')
    env = target_environment(arch)
    subprocess.run(['xcrun', '--find', 'clang++'], env=env, check=True)
    python = prepare_python_environment(env)
    subprocess.run([python, str(ROOT / 'lynxtron_tools/prepare_build_env.py')],
                   cwd=ROOT, env=env, check=True)
    subprocess.run([str(ROOT / 'lynxtron_tools/hab'), 'sync', '.',
                    '--no-history', '--target', 'extension', '--target-only'],
                   cwd=ROOT / 'src', env=env, check=True)
    subprocess.run([python, 'setup_deps.py'],
                   cwd=ROOT / 'lynx/third_party/weak-node-api', env=env, check=True)
    subprocess.run(['node', '--version'], env=env, check=True)
    subprocess.run(['cmake', '--version'], env=env, check=True)
    return env
