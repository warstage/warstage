#!/usr/bin/env bash

# Copyright Felix Ungman. All rights reserved.
# Licensed under GNU General Public License version 3 or later.

version=1.76.0
_version=_${version//./_}

if [ -x boost${_version} ]; then
    echo "boost already installed"
else
    wget https://boostorg.jfrog.io/artifactory/main/release/${version}/source/boost${_version}.tar.gz
    tar -xf boost${_version}.tar.gz
    rm boost${_version}.tar.gz
fi
