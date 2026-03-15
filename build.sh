#!/usr/bin/env bash

if [ -n "$SUDO_USER" ]; then
	echo "This script must not be run with sudo."
	exit 1
fi

set -eu

cd "$(dirname "$0")"

BIN_DIR="$(pwd)/bin"
FLAGS="-Wall -Wextra -Wconversion -Wno-unused -pedantic -std=c11"
OPTIMIZE_FLAGS="-g -O0"

DEPS_INCLUDE_DIR="$(pwd)/deps/install/include"
DEPS_LIB_DIR="$(pwd)/deps/install/lib"

mkdir -p "$BIN_DIR"

clang $FLAGS $OPTIMIZE_FLAGS \
	-isystem "$DEPS_INCLUDE_DIR" \
	code/main.c \
	-L"$DEPS_LIB_DIR" \
	-lglfw3\
	-lm \
	-o "$BIN_DIR/ttf"
