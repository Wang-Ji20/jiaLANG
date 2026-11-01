#!/usr/bin/env bash
# Installs LLVM <version> from apt.llvm.org on Ubuntu, plus what jiaLang
# needs to build against it.
#
#   install-llvm.sh <version>               LLVM libraries, clang, and build tools
#   install-llvm.sh <version> clang-format  clang-format only
set -euo pipefail

version=$1
what=${2:-all}

curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh
sudo bash /tmp/llvm.sh "$version"

if [ "$what" = clang-format ]; then
  sudo apt-get install -y "clang-format-$version"
  exit 0
fi

# LLVM's CMake package refers to these libraries, so they must be present
# even though jiaLang does not use them directly.
sudo apt-get install -y \
  "llvm-$version-dev" ninja-build \
  zlib1g-dev libzstd-dev libxml2-dev libedit-dev libffi-dev libcurl4-openssl-dev
