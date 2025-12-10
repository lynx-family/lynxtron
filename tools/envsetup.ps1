# Copyright 2025 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$tools_path = Split-Path -Parent $MyInvocation.MyCommand.Path
$lynxtron_dir_path = Split-Path -Parent $tools_path

function Lynxtron-Env-Setup {
    $buildtoolsDir = Join-Path $lynxtron_dir_path 'buildtools'
    $env:PATH += Join-Path $buildtoolsDir 'gn;'
    $env:PATH += Join-Path $buildtoolsDir 'ninja;'
}

Lynxtron-Env-Setup

