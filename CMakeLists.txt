cmake_minimum_required(VERSION 3.22)
set(IDF_TARGET "esp32s31" CACHE STRING "Espressif chip target")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(MINIMAL_BUILD ON)
project(jaxcore-doom)
