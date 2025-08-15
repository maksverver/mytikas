#!/bin/sh
#
# Builds the Javascript WASM API. Requires Emscripten to be installed!
#
# This generates wasm-api.wasm and wasm-api.js, and copies the files to the www/
# subdirectory.

set -e -E -o pipefail

cd "$(dirname "$(readlink -f "$0")")"

BUILD_DIR=build-wasm-api
TOOLCHAIN=/usr/lib/emscripten/cmake/Modules/Platform/Emscripten.cmake
NODE=/usr/bin/node

if ! [ -d "$BUILD_DIR" ]; then
    # Equivalent to:
    #
    #/usr/lib/emscripten/emcmake cmake -B "$BUILD_DIR" -D CMAKE_BUILD_TYPE=Release
    #
    cmake \
           -B "$BUILD_DIR" \
           -D CMAKE_BUILD_TYPE=Release \
           -D CMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
           -D CMAKE_CROSSCOMPILING_EMULATOR="$NODE"
fi

cmake --build "$BUILD_DIR" --target wasm-api

cp -f -l "$BUILD_DIR"/apps/wasm-api.{js,wasm} www/
