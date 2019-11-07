#!/bin/bash

set -e

if [[ "$OSTYPE" == "darwin"* ]]; then
  echo "Not trying to setup Arch on macOS"
else
  curl -s https://raw.githubusercontent.com/mikkeloscar/arch-travis/master/arch-travis.sh | bash
fi
