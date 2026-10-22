#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
out="${1:-$root/bin/forge2-engine}"
mkdir -p -- "$(dirname -- "$out")"
sources=(bench bitboard endgame eval main movegen movepick pawns position
         search tests timeman tt uci forge_next_probe)
args=()
for source in "${sources[@]}"; do args+=("$root/src/$source.cpp"); done
# Fixed CPU target, matching the pilot's native AVX2 implementation.
# The former netB evaluator is deliberately not linked.
"${CXX:-g++}" -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -flto=1 \
    -DFORGE_NEXT_PROBE -DFORGE_NEXT_OPT=2 -DFORGE_CACHE_AUDIT \
    -march=haswell -mtune=generic -frandom-seed=forge2-placement-policy \
    "-ffile-prefix-map=$root=." -pthread -iquote "$root/src" \
    "${args[@]}" "$root/src/syzygy/tbprobe.cpp" -o "$out.tmp"
mv -- "$out.tmp" "$out"
printf '%s\n' "$out"
