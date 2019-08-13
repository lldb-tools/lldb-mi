#!/bin/bash

set -e

# cd to source directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"
cd ..

# Clang format check
bash ci/clang-format.sh

# Creating build directory
mkdir build
cd build

# Build lldb-mi
cmake  -GNinja ..
ninja
