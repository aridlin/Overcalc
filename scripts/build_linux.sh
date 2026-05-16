#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j
ctest --test-dir "$BUILD_DIR" --output-on-failure -C "$BUILD_TYPE"

echo "Linux build complete: $BUILD_DIR/overcalc"
