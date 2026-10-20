#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
mkdir -p "$root/bin"
sources=()
for s in bitboard endgame eval pawns position movegen forge_factor_net; do
    sources+=("$root/src/$s.cpp")
done
"${CXX:-g++}" -std=c++20 -O3 -DNDEBUG -DFORGE_MINI -DFORGE_WIDE_NETB \
    -march=haswell -mtune=generic "-ffile-prefix-map=$root=." \
    -iquote "$root/src" -iquote "$root/tools" "${sources[@]}" \
    "$root/src/syzygy/tbprobe.cpp" "$root/tools/extract_training.cpp" -o "$root/bin/forge-extract"
