#!/bin/bash

set -e

echo "Checking clang-format"
find -name *.h -or -name *.cpp | xargs clang-format -i
git diff --exit-code
