cmake_minimum_required(VERSION 3.13)

message("Hej: ${CMAKE_OSX_SYSROOT}")

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

project(warstage)

add_subdirectory("engine" "engine-output")
