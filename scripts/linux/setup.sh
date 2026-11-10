#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."

if [ -z "${XCLANG_ROOT:-}" ]; then
  cmake "-DXCLANG_INSTALL_DIR=$PWD/build/xclang-sdk" -P scripts/ci/install-xclang.cmake
  XCLANG_ROOT="$PWD/build/xclang-sdk/xclang"
fi
export XCLANG_ROOT
export PATH="$XCLANG_ROOT/bin:$PATH"
case "$(uname -m)" in
  aarch64|arm64) preset=linux-arm-release ;;
  x86_64) preset=linux-release ;;
  *) echo "Unsupported Linux architecture" >&2; exit 1 ;;
esac
mkdir -p dist
cp scripts/linux/sbinit.config dist/
cmake -S source --preset "$preset"
if [ "$#" -ne 0 ]; then
  cmake --build "build/$preset" --parallel "$1"
  ./dist/asset_packer -version
  ./dist/asterbound_server -version
fi
