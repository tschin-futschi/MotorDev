@echo off
REM ===========================================================================
REM build_local.bat - MotorDev local Debug full rebuild (MinGW + Qt 6.11.0)
REM
REM Always wipes build_local\ then does a full rebuild.
REM Works the same whether double-clicked from Explorer or run from cmd.
REM ===========================================================================

setlocal enableextensions

set "QT_ROOT=D:\Qt\6.11.0\mingw_64"
set "MINGW_ROOT=D:\Qt\Tools\mingw1310_64"
set "CMAKE_ROOT=D:\Qt\Tools\CMake_64"
set "BUILD_DIR=build_local"
set "PATH=%MINGW_ROOT%\bin;%QT_ROOT%\bin;%CMAKE_ROOT%\bin;%PATH%"

echo.
echo === MotorDev local full rebuild ===
echo   QT_ROOT    = %QT_ROOT%
echo   MINGW_ROOT = %MINGW_ROOT%
echo   BUILD_DIR  = %BUILD_DIR%
echo   Time       = %DATE% %TIME%
echo.

REM -------- Sanity check toolchain --------
if not exist "%QT_ROOT%\lib\cmake\Qt6" (
    echo [ERROR] Qt6 not found at: %QT_ROOT%\lib\cmake\Qt6
    pause
    exit /b 1
)
if not exist "%MINGW_ROOT%\bin\g++.exe" (
    echo [ERROR] MinGW g++ not found at: %MINGW_ROOT%\bin\g++.exe
    pause
    exit /b 1
)
if not exist "%CMAKE_ROOT%\bin\cmake.exe" (
    echo [ERROR] CMake not found at: %CMAKE_ROOT%\bin\cmake.exe
    pause
    exit /b 1
)

REM -------- Clean --------
echo [clean] removing %BUILD_DIR% ...
if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
if errorlevel 1 (
    echo [ERROR] failed to remove %BUILD_DIR%
    pause
    exit /b 1
)

REM -------- Configure --------
echo [cmake] configuring ...
"%CMAKE_ROOT%\bin\cmake.exe" ^
    -S . ^
    -B %BUILD_DIR% ^
    -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
    -DQt6_DIR="%QT_ROOT%\lib\cmake\Qt6" ^
    -DCMAKE_CXX_COMPILER="%MINGW_ROOT%\bin\g++.exe" ^
    -DCMAKE_MAKE_PROGRAM="%MINGW_ROOT%\bin\mingw32-make.exe"
if errorlevel 1 (
    echo.
    echo [ERROR] cmake configure failed
    pause
    exit /b 1
)

REM -------- Build --------
echo.
echo [make] building (-j4) ...
"%MINGW_ROOT%\bin\mingw32-make.exe" -C %BUILD_DIR% -j4
if errorlevel 1 (
    echo.
    echo [ERROR] build failed
    pause
    exit /b 1
)

REM -------- Report --------
set "EXE_PATH=%BUILD_DIR%\MotorDev.exe"
if not exist "%EXE_PATH%" (
    echo.
    echo [WARN] build reported success but %EXE_PATH% not found
    pause
    exit /b 1
)

echo.
echo ============================================================
echo Build OK
for %%I in ("%EXE_PATH%") do echo   exe : %%~fI (%%~zI bytes)
echo   time: %DATE% %TIME%
echo ============================================================
echo.

pause
endlocal
