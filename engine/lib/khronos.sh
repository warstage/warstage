#!/usr/bin/env bash

# Copyright Felix Ungman. All rights reserved.
# Licensed under GNU General Public License version 3 or later.

if [ -x OpenGL ]; then
    echo "OpenGL already installed"
else
    mkdir -p khronos/GLES3
    mkdir -p khronos/KHR
    cd khronos/GLES3
    wget --quiet https://www.khronos.org/registry/OpenGL/api/GLES3/gl3.h
    wget --quiet https://www.khronos.org/registry/OpenGL/api/GLES3/gl3platform.h
    cd ../KHR
    wget --quiet https://www.khronos.org/registry/EGL/api/KHR/khrplatform.h
fi