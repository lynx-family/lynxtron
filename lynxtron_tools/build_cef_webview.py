#!/usr/bin/env python3
"""Prepare, build and validate macOS CEF artifacts on either host architecture."""
import argparse
from pathlib import Path
import platform
import re
import subprocess
import tempfile

from prepare_mac_cross_compile_env import prepare

ROOT = Path(__file__).resolve().parent.parent


def run(*args, cwd=ROOT, env=None):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=['arm64', 'x64'], required=True)
    parser.add_argument('--version', help='Also create a release zip in publish/')
    args = parser.parse_args()
    if args.version and not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?', args.version):
        parser.error('--version must be a release version, not a path')
    if platform.system() != 'Darwin':
        parser.error('This entry point requires macOS and Xcode Command Line Tools')
    env = prepare(args.arch)
    sdk = ROOT / 'third_party/cef_binary'
    target = 'x86_64' if args.arch == 'x64' else 'arm64'
    run('lipo', sdk / 'Release/Chromium Embedded Framework.framework/Chromium Embedded Framework', '-verify_arch', target)
    src = ROOT / 'src'
    run('node', 'tools/yarn.js', 'workspace', '@lynx-js/cef-webview', 'build', cwd=src, env=env)
    output = src / 'packages/cef-webview/dist/darwin' / args.arch
    binaries = [output / 'cef_extension.node']
    binaries += list(output.glob('frameworks/*.app/Contents/MacOS/*'))
    binaries += list(output.glob('frameworks/*.framework/Chromium Embedded Framework'))
    if len(binaries) < 3:
        raise RuntimeError('Missing framework or helper app in staged output')
    for binary in binaries:
        run('lipo', binary, '-verify_arch', target)
        run('file', binary)
    if args.version:
        import shutil
        deps = Path(tempfile.mkdtemp(prefix='lynxtron-cef-release-'))
        name = f'cef_webview-v{args.version}-darwin-{args.arch}'
        staging = deps / name
        shutil.copytree(output, staging, symlinks=True)
        publish = ROOT / 'publish'
        publish.mkdir(exist_ok=True)
        # Create a fresh archive so a repeated version cannot retain stale files.
        run('zip', '-qry', deps / f'{name}.zip', name, cwd=deps)
        shutil.move(str(deps / f'{name}.zip'), publish / f'{name}.zip')
    print(f'Validated {args.arch} artifacts: {output}', flush=True)


if __name__ == '__main__':
    main()
