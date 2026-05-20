@echo off

set QT_ROOT=C:\Qt\6.11.0\mingw_64
set MINGW_ROOT=C:\Qt\Tools\mingw1310_64
set CMAKE_ROOT=C:\Qt\Tools\CMake_64
set BUILD_DIR=build_release
set PATH=%MINGW_ROOT%\bin;%QT_ROOT%\bin;%CMAKE_ROOT%\bin;%PATH%

"%CMAKE_ROOT%\bin\cmake.exe" ^
  -S . ^
  -B %BUILD_DIR% ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
  -DQt6_DIR="%QT_ROOT%\lib\cmake\Qt6" ^
  -DCMAKE_CXX_COMPILER="%MINGW_ROOT%\bin\g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="%MINGW_ROOT%\bin\mingw32-make.exe"

"%MINGW_ROOT%\bin\mingw32-make.exe" -C %BUILD_DIR% -j4

"%QT_ROOT%\bin\windeployqt.exe" ^
  --release ^
  --no-translations ^
  --no-system-d3d-compiler ^
  --no-opengl-sw ^
  %BUILD_DIR%\MotorDev.exe

echo.
echo ============================================================
echo Release build done: %BUILD_DIR%\MotorDev.exe
echo Qt dependencies deployed by windeployqt.
echo.
echo Manual deployment still required (not auto-copied):
echo   - (Removed 2026-05-19) AW86100.dll / libfftw3-3.dll
echo     Flashing moved to STM32 local ISP (protocol 0x32~0x37).
echo   - MSVC/MFC runtime: mfc140u.dll / MSVCP140.dll /
echo                       VCRUNTIME140.dll / VCRUNTIME140_1.dll
echo ============================================================
