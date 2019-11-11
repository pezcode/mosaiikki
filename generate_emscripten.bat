@echo off

REM you need ninja in your PATH

REM run emsdk_env beforehand so emcc is in PATH
REM or set -D EMSCRIPTEN_PREFIX

REM crosscompiling requires a native build of corrade-rc
REM you have to compile it seperately and add it to PATH
REM or set -D CORRADE_RC_EXECUTABLE

mkdir build_emscripten & cd build_emscripten

set TOOLCHAIN=..\3rdparty\magnum\toolchains\generic\Emscripten-wasm.cmake
cmake ^
  -G Ninja ^
  -D CMAKE_TOOLCHAIN_FILE=%TOOLCHAIN% ^
  ..
cd ..
