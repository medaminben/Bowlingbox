#!/usr/bin/env bash
# Installs Gamebox from an existing build tree into <build-dir>/install-smoke
# and builds and runs a separate consumer project against it via find_package.
# Everything stays inside the build directory.
set -euo pipefail

build_dir=${1:?usage: ci/install-smoke.sh <build-dir>}
here=$(cd "$(dirname "$0")" && pwd)
work="$(cd "$build_dir" && pwd)/install-smoke"
rm -rf "$work"

cmake --install "$build_dir" --prefix "$work/prefix"
cmake -S "$here/consumer" -B "$work/consumer" -DCMAKE_PREFIX_PATH="$work/prefix"
cmake --build "$work/consumer"
LD_LIBRARY_PATH="$work/prefix/lib:$work/prefix/lib64" "$work/consumer/consumer"
echo "install smoke test passed"
