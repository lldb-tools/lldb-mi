#!/bin/bash

# cd to source directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"
cd ..

# Creating build directory
mkdir build
cd build

ls /usr/lib/cmake/llvm/

# Build lldb-mi
cmake -DLLVM_DIR="/usr/lib/cmake/llvm/" -GNinja ..
ninja
