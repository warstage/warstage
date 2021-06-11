#!/usr/bin/env bash

# Copyright Felix Ungman. All rights reserved.
# Licensed under GNU General Public License version 3 or later.

version=1.76.0
boost_version=boost_${version//./_}

if [ -x boost-${version} ]; then
    echo "boost already installed"
else
    wget --quiet https://boostorg.jfrog.io/artifactory/main/release/${version}/source/${boost_version}.tar.gz
    tar -xf ${boost_version}.tar.gz
    rm ${boost_version}.tar.gz
    mv ${boost_version} boost-${version}
fi
