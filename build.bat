@echo off
set BUILD_DIR=.\build\

:: build for win32
if not exist %BUILD_DIR%\win32 mkdir %BUILD_DIR%\win32
cd %BUILD_DIR%\win32
cmake -A Win32 -S ..\..
cmake --build . --config Release

cd ..\..

:: build for win64
if not exist %BUILD_DIR%\win64 mkdir %BUILD_DIR%\win64
cd %BUILD_DIR%\win64
cmake -A x64 -S ..\..
cmake --build . --config Release
