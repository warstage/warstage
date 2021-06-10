cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

project(warstage)

add_subdirectory("engine" "engine-output")
