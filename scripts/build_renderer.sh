#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
renderer_dir="$repo_root/renderer"
build_dir="$renderer_dir/build"
build_type=${CMAKE_BUILD_TYPE:-Release}

if ! command -v cmake >/dev/null 2>&1; then
	echo "rndr.nvim: cmake is required to build the renderer." >&2
	exit 1
fi

if command -v nproc >/dev/null 2>&1; then
	jobs=$(nproc)
elif command -v getconf >/dev/null 2>&1; then
	jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
elif command -v sysctl >/dev/null 2>&1; then
	jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 1)
else
	jobs=1
fi

printf 'Building rndr renderer in %s (%s)\n' "$build_dir" "$build_type"

cmake -S "$renderer_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type"
cmake --build "$build_dir" --parallel "$jobs" --config "$build_type"
