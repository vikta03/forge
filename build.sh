#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
out="${1:-$root/bin/forge-engine}"
mkdir -p -- "$(dirname -- "$out")"
sources=(bench bitboard endgame eval main movegen movepick pawns position
         search tests timeman tt uci forge_factor_net)
args=()
for source in "${sources[@]}"; do args+=("$root/src/$source.cpp"); done
audit=()
if [[ "${FORGE_CACHE_AUDIT:-0}" == 1 ]]; then audit+=(-DFORGE_CACHE_AUDIT); fi
# Haswell fixes the ISA contract (AVX2, BMI1/2, FMA), regardless of the build host.
"${CXX:-g++}" -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -flto=1 \
    -DFORGE_MINI -DFORGE_WIDE_NETB "${audit[@]}" \
    -march=haswell -mtune=generic -frandom-seed=forge-netB-1.0.0 \
    "-ffile-prefix-map=$root=." -pthread -iquote "$root/src" \
    "${args[@]}" "$root/src/syzygy/tbprobe.cpp" -o "$out"
printf '%s\n' "$out"
