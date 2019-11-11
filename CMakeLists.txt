cmake_minimum_required(VERSION 3.7)
project(calculi CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

# find external SDL2 before Magnum finds it with its own FindSDL2.cmake
# Magnum expects a dynamic library and doesn't add required libraries
if(NOT EMSCRIPTEN)
    find_package(SDL2 REQUIRED)
endif()
# we don't install Corrade/Magnum (and its CMake config files) so we need to point
# CMake at the FindXXX.cmake files to find them via find_package
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/3rdparty/magnum/modules/")

add_subdirectory(3rdparty)
add_subdirectory(src)
