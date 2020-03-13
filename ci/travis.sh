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

# Homebrew repository does not contain Google Test
INCLUDE_TESTS=`[[ "$OSTYPE" == "darwin"* ]] && echo OFF || echo ON`

# Build lldb-mi
cmake $1 -GNinja -DINCLUDE_TESTS=$INCLUDE_TESTS ..
ninja

# Run tests
if [[ $INCLUDE_TESTS == OFF ]]; then
  echo "Not running tests"
else
  cd test/unittests
  ctest -V
fi
