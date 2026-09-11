# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$tools_path = Split-Path -Parent $MyInvocation.MyCommand.Path
$lynxtron_dir_path = Split-Path -Parent $tools_path
$root_dir_path = Split-Path -Parent $lynxtron_dir_path

function Lynxtron-Env-Setup {
    $buildtoolsDir = Join-Path $root_dir_path 'buildtools'
    Write-Host "buildtoolsDir: " $buildtoolsDir
    $env:PATH += ';'
    $env:PATH += Join-Path $buildtoolsDir 'gn;'
    $env:PATH += Join-Path $buildtoolsDir 'ninja;'
    $env:PATH += Join-Path $buildtoolsDir 'sccache;'

    # Habitat syncs Chromium's pinned resource compiler into the repository.
    # Prefer it over a runner- or machine-provided Windows SDK copy so GN's
    # bare `rc.exe` invocation is deterministic.
    $resourceCompiler = Join-Path $lynxtron_dir_path 'build\toolchain\win\rc\win\rc.exe'
    if (Test-Path -LiteralPath $resourceCompiler -PathType Leaf) {
        $resourceCompilerDir = Split-Path -Parent $resourceCompiler
        $env:PATH = "$resourceCompilerDir;$env:PATH"
        Write-Host "resourceCompilerDir: " $resourceCompilerDir
    }
}

function Python-Env-Setup {
  python $lynxtron_dir_path\lynxtron_tools\vpython_tools\vpython_env_setup.py --root_dir $lynxtron_dir_path
  $venv_path = Join-Path $lynxtron_dir_path '.venv'
  & $venv_path\Scripts\Activate.ps1
}

Lynxtron-Env-Setup
Python-Env-Setup
