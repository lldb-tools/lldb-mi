#!/bin/bash

set -e

echo "Checking clang-format"
cd src
clang-format *.h *.cpp -i
git diff --exit-code
