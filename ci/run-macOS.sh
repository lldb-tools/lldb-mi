#!/bin/bash

set -e

if [[ "$OSTYPE" == "darwin"* ]]; then
  export HOMEBREW_NO_INSTALL_CLEANUP=1
  brew update
  brew install ninja
  curl https://teemperor.de/pub/lldb-inst-min.tar.xz --output lldb-inst.tar.xz
  tar xJf lldb-inst.tar.xz
  inst_path=`realpath llvm-inst`
  bash ci/travis.sh "-DCMAKE_PREFIX_PATH=$inst_path"
else
  echo "Nothing to do for non-Darwin systems"
fi
