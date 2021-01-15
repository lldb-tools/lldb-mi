#!/bin/bash

set -e

echo "Checking clang-format"
find -name *.h -or -name *.cpp | xargs clang-format -i -style=llvm
git diff --exit-code
