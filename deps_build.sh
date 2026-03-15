#!/usr/bin/env bash

if [ -n "$SUDO_USER" ]; then
	echo "This script must not be run with sudo."
	exit 1
fi

set -eu

cd "$(dirname "${BASH_SOURCE[0]}")"
cd deps

INSTALL_DIR="$(pwd)/install"

# ensure clean install dir
rm -rf install
mkdir install

# build glfw

cd glfw

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DGLFW_BUILD_WAYLAND=ON \
	-DGLFW_BUILD_EXAMPLES=OFF \
	-DGLFW_BUILD_TESTS=OFF \
	-DGLFW_BUILD_DOCS=OFF \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

cmake --build build
cmake --install build

cd ..

cp gl.h "$INSTALL_DIR"/include/gl.h
