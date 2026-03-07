#!/usr/bin/env bash

if [ -n "$SUDO_USER" ]; then
	echo "This script must not be run with sudo."
	exit 1
fi

set -eu

cd "$(dirname "${BASH_SOURCE[0]}")"
cd deps

# ensure clean install dir
rm -rf install
mkdir install

# build raylib 5.5
cd raylib-5.5

cmake -S . -B build \
	-DGLFW_BUILD_WAYLAND=ON \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_INSTALL_PREFIX="$(pwd)/../install"

cmake --build build
cmake --install build

cd ..
