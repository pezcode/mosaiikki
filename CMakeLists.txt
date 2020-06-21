cmake_minimum_required(VERSION 3.4)
project(mosaiikki CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE Release)
endif()

set(BUILD_SHARED_LIBS OFF)

# we don't install Corrade/Magnum/Plugins (and their CMake config files) so we need to point
# CMake at the FindXXX.cmake files to find them via find_package
# MagnumPlugins has all of them, except MagnumIntegration (which is missing MagnumPlugins)
list(APPEND CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/3rdparty/magnum-plugins/modules"
    "${PROJECT_SOURCE_DIR}/3rdparty/magnum-integration/modules"
)

add_subdirectory(3rdparty)
add_subdirectory(src)
