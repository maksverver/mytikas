#!/bin/sh
#
# Builds the Javascript WASM API. Requires emcmake to be installed in
# /usr/lib/emscripten or in another directory that is added to PATH.
#
# This generates wasm-api.wasm and wasm-api.js, and copies the files to the www/
# subdirectory.

set -e

cd "$(dirname "$(readlink -f "$0")")"

BUILD_DIR=build-wasm-api
INSTALL_DIR=www/generated
PATH=/usr/lib/emscripten:${PATH}

if ! [ -d "$BUILD_DIR" ]; then
    emcmake cmake -B "$BUILD_DIR" -D CMAKE_BUILD_TYPE=Release

    # Should be equivalent to:
    #cmake \
    #       -B "$BUILD_DIR" \
    #       -D CMAKE_BUILD_TYPE=Release \
    #       -D CMAKE_TOOLCHAIN_FILE=/usr/lib/emscripten/cmake/Modules/Platform/Emscripten.cmake \
    #       -D CMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/node
fi

cmake --build "$BUILD_DIR" --target wasm-api

cp -f -l \
    "$BUILD_DIR"/apps/wasm-api.js \
    "$BUILD_DIR"/apps/wasm-api.wasm \
    www/generated/
