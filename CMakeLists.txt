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

# we don't install Corrade/Magnum/Plugins (and their CMake config files) so we need to point
# CMake at the FindXXX.cmake files to find them via find_package
# MagnumPlugins has all of them, except MagnumIntegration (which is missing MagnumPlugins)
list(APPEND CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/3rdparty/magnum-plugins/modules"
    "${PROJECT_SOURCE_DIR}/3rdparty/magnum-integration/modules"
    "${PROJECT_SOURCE_DIR}/3rdparty/ucm/cmake"
)

include(ucm)

# build static libs, use static C++ runtime
# vcpkg compiles static libs with static runtime
# if we don't use the same setup we will get two different runtimes
set(BUILD_SHARED_LIBS OFF)
# CMake 3.15+ has support for this
# https://cmake.org/cmake/help/v3.15/prop_tgt/MSVC_RUNTIME_LIBRARY.html
# we use ucm for compatibility with older versions
ucm_set_runtime(STATIC)

add_subdirectory(3rdparty)
add_subdirectory(src)
