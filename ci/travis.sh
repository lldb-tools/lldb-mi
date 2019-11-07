#!/bin/bash

set -e

# cd to source directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"
cd ..

# Clang format check
if [[ "$OSTYPE" == "darwin"* ]]; then
  echo "Not running clang-format on macOS"  # Clang format check already done on Arch
else
  bash ci/clang-format.sh
fi

# Creating build directory
mkdir build
cd build

# Build lldb-mi
cmake $1 -GNinja ..
ninja
