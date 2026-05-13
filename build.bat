@echo off
 
set QT_ROOT=C:\Qt\6.11.0\mingw_64
set MINGW_ROOT=C:\Qt\Tools\mingw1310_64
set CMAKE_ROOT=C:\Qt\Tools\CMake_64
set PATH=%MINGW_ROOT%\bin;%QT_ROOT%\bin;%CMAKE_ROOT%\bin;%PATH%

"%CMAKE_ROOT%\bin\cmake.exe" ^
  -S . ^
  -B build_make_qt ^
  -G "MinGW Makefiles" ^
  -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
  -DQt6_DIR="%QT_ROOT%\lib\cmake\Qt6" ^
  -DCMAKE_CXX_COMPILER="%MINGW_ROOT%\bin\g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="%MINGW_ROOT%\bin\mingw32-make.exe"
 
"%MINGW_ROOT%\bin\mingw32-make.exe" -C build_make_qt -j4
