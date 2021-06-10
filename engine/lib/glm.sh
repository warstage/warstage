#!/usr/bin/env bash

# Copyright Felix Ungman. All rights reserved.
# Licensed under GNU General Public License version 3 or later.

version=0.9.9.8

if [ -x glm-${version} ]; then
    echo "glm already installed"
else
    wget --quiet https://github.com/g-truc/glm/archive/${version}.zip
    unzip -q ${version}.zip
    rm ${version}.zip
fi
