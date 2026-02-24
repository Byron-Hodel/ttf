#!/usr/bin/env bash

if [ -n "$SUDO_USER" ]; then
	echo "This script must not be run with sudo."
	exit 1
fi

set -eu

cd "$(dirname "$0")"

BIN_DIR="$(pwd)/bin"
FLAGS="-Wall -Wextra -Wconversion -pedantic -std=c11"
OPTIMIZE_FLAGS="-g -O0"

mkdir -p "$BIN_DIR"

clang $FLAGS $OPTIMIZE_FLAGS -o"$BIN_DIR/ttf" code/main.c
