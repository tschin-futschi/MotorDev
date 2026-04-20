@echo off
set MINGW_PATH=D:\Qt\Tools\mingw1310_64\bin
set BUILD_DIR=%~dp0build_make_qt
set PATH=%MINGW_PATH%;%PATH%

cd /d "%BUILD_DIR%"
"%MINGW_PATH%\mingw32-make.exe" -j8
if %ERRORLEVEL% EQU 0 (
    echo.
    echo BUILD SUCCESS
) else (
    echo.
    echo BUILD FAILED
)
pause
