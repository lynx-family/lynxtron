# Build CEF after the Windows Lynxtron source runtime has been built.
param(
    [ValidateSet('x64')]
    [string]$Arch = 'x64',
    [string]$ImportLibrary
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
if (-not $ImportLibrary) {
    $ImportLibrary = Join-Path $workspace 'out\Release\lynxtron.dll.lib'
}
if (-not (Test-Path -LiteralPath $ImportLibrary -PathType Leaf)) {
    throw "Missing source-built Lynxtron import library: $ImportLibrary. Build the Windows runtime first."
}
$ImportLibrary = (Resolve-Path -LiteralPath $ImportLibrary).Path

$environment = @{
    PATH = "C:\cmake\bin;$workspace\buildtools\node;$workspace\buildtools\gn;$workspace\buildtools\ninja;$env:PATH"
    vs2022_install = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
    DEPOT_TOOLS_WIN_TOOLCHAIN = '0'
    GYP_MSVS_OVERRIDE_PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
    WINDOWSSDKDIR = "${env:ProgramFiles(x86)}\Windows Kits\10"
    npm_config_platform = 'win32'
    npm_config_arch = $Arch
    LYNXTRON_IMPORT_LIB = $ImportLibrary
}
$previous = @{}
Push-Location (Join-Path $workspace 'src')
try {
    foreach ($name in $environment.Keys) {
        $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $environment[$name], 'Process')
    }
    # Isolate Habitat's stderr capture from this script's Stop error policy.
    powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PSScriptRoot\hab.ps1" sync . -f --no-history --target extension --target-only
    if ($LASTEXITCODE -ne 0) { throw 'Failed to sync the CEF extension dependencies' }
    node tools/yarn.js workspace @lynx-js/cef-webview build
    if ($LASTEXITCODE -ne 0) { throw 'Failed to build the Windows CEF webview' }
    foreach ($file in @('cef_extension.node', 'cef_subprocess.exe', 'libcef.dll')) {
        if (-not (Test-Path -LiteralPath "packages\cef-webview\dist\win32\$Arch\$file" -PathType Leaf)) {
            throw "Windows CEF webview build did not stage $file"
        }
    }
} finally {
    foreach ($name in $previous.Keys) {
        [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process')
    }
    Pop-Location
}
