#!/usr/bin/env bash
# Builds the club music synthesizer: Tools/Music/build.sh <output exe>
# Then: <exe> <out dir>   (writes MUS_Club_*.wav stems and Preview_*.wav mixes; see MUSIC.md)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:-/tmp/clubsynth}"
g++ -std=c++17 -O2 -Wall -Wextra -Wshadow -Werror -o "$out" "$here/clubsynth.cpp"
echo "built $out"
