echo Setting up environment with vcvars64.bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

where mt.exe
if errorlevel 1 (
    echo mt.exe not found in PATH
    exit /b 1
) else (
    echo mt.exe found successfully
)

npx cmake-js compile