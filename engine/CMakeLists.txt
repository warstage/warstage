# Copyright Felix Ungman. All rights reserved.
# Licensed under GNU General Public License version 3 or later.

cmake_minimum_required(VERSION 3.13)
project(warstage-engine)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS true)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_SWIZZLE")

find_package(ZLIB REQUIRED)
find_package(OpenGL REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(.
        ${ZLIB_ICLUDE_DIRS}
        lib/boost_1_76_0
        lib/glm-0.9.9.8
        src
        )

link_directories(
        ${OPENGL_INCLUDE_DIRS}
)

link_libraries(
        ${OPENGL_LIBRARIES}
        ${ZLIB_LIBRARIES}
        Threads::Threads
)

add_executable(warstage-engine
        main.cpp
        src/value/builder.test.cpp
        src/value/compressor.cpp
        src/value/compressor.test.cpp
        src/value/decompressor.cpp
        src/value/json.cpp
        src/value/json.test.cpp
        src/value/value.cpp
        src/value/value.test.cpp
        src/utilities/dictionary.cpp
        )

add_test(warstage-engine warstage-engine
        --logger=HRF,all
        --color_output=false
        --report_format=HRF
        --show_progress=no
        )