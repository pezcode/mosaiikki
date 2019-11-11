@echo off

REM vcpkg is used to link to static SDL2

if not defined VCPKG_ROOT (
  echo vcpkg not found, please set VCPKG_ROOT
  exit
)
set VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

mkdir build & cd build

cmake ^
  -G "Visual Studio 16 2019" ^
  -A x64 ^
  -D CMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
  -D VCPKG_TARGET_TRIPLET=x64-windows-static ^
  ..
cd ..
