@echo off
python "%~dp0..\git_cache_guard.py" %*
exit /b %ERRORLEVEL%
