#!/usr/bin/env bash
# Builds the map generator test harness outside Unreal. Strict warnings, to catch what UE's compilers would flag.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../GameJam/Source/GameJam/MapGen"
out="${1:-$here/mapgen_test}"
g++ -std=c++17 -O2 -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Werror \
	-I"$src" "$src/GJLayoutGen.cpp" "$here/mapgen_test.cpp" -o "$out"
echo "built $out"
