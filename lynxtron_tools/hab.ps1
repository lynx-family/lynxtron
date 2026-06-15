[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$__version__ = "0.3.146-alpha.3"

if ($null -eq $env:HABITAT_VERSION) { 
    $HABITAT_VERSION = $__version__
}
else {
    $HABITAT_VERSION = $env:HABITAT_VERSION
}

$HABITAT_RELEASE_BASE_URL = "https://github.com/lynx-family/habitat/releases"

if ('latest' -eq $HABITAT_VERSION) {
    $HABITAT_DOWNLOAD_URL = "${HABITAT_RELEASE_BASE_URL}/latest/download/hab.exe"
    $HABITAT_VERSION = (curl.exe -sL -w '%{url_effective}' ${HABITAT_RELEASE_BASE_URL}/latest) `
    | Select-Object -Last 1 `
    | Select-String -Pattern '([0-9]+\.[0-9]+\.[0-9]{1,3})'
    $HABITAT_VERSION = $HABITAT_VERSION.Matches.Value
    Write-Debug "Using latest version: ${HABITAT_VERSION}"
}
else {
    $HABITAT_DOWNLOAD_URL = "${HABITAT_RELEASE_BASE_URL}/download/$HABITAT_VERSION/hab.exe"
}

$HABITAT_CACHE_DIR = "${HOME}\.habitat\bin"
$HABITAT_BIN = "${HABITAT_CACHE_DIR}\hab-${HABITAT_VERSION}.exe"

function install() {
    if (-Not (Test-Path -Path $HABITAT_CACHE_DIR)) {
        New-Item -ItemType Directory -Force -Path $HABITAT_CACHE_DIR
    }
    if (-Not (Test-Path -Path $HABITAT_BIN)) {
        Write-Output "Installing habitat ($HABITAT_VERSION) from url $HABITAT_DOWNLOAD_URL..."
        curl.exe -sL -# $HABITAT_DOWNLOAD_URL -o $HABITAT_BIN
        try {
            $byteCount = 3072
            $stream = [System.IO.File]::OpenRead($HABITAT_BIN)
            $buffer = New-Object byte[] $byteCount
            $bytesRead = $stream.Read($buffer, 0, $byteCount)
            $stream.Close()

            Write-Host "$bytesRead byte(hex)"
            for ($i = 0; $i -lt $bytesRead; $i++) {
                $hex = $buffer[$i].ToString("X2")
                Write-Host -NoNewline "$hex "
                if (($i + 1) % 16 -eq 0) {
                    Write-Host
                }
            }
            Write-Host
        }
        catch {
            Write-Error "Read file failed: $_"
            exit 1
        }
    }
}

install

$CURRENT_VERSION = (& $HABITAT_BIN -v)

foreach ($arg in $args) {
    if ($arg -eq "sync") {
        $is_sync_command = $true
    }
    if (Test-Path -Path $arg) {
        $root_dir = $arg
    }
}

$exit_code = 0
if ($is_sync_command) {
    $err_msg = (& $HABITAT_BIN deps $root_dir 2>&1)
    $exit_code = $LastExitCode
    if ($exit_code -eq 126) {
        $HABITAT_VERSION = (& Select-String -InputObject $err_msg -Pattern '([0-9]+\.[0-9]+\.[0-9]{1,3})$')
        $HABITAT_VERSION = $HABITAT_VERSION.Matches.Value
        Write-Output "required version ${HABITAT_VERSION} does not exist, try installing it first (current version is $CURRENT_VERSION)"
        $HABITAT_BIN = "${HABITAT_CACHE_DIR}\hab-${HABITAT_VERSION}.exe"
        install
    }
}

& $HABITAT_BIN @args
$exit_code = $LASTEXITCODE
exit $exit_code
