#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
variant="${1:-wide192}"
flags=(-DFORGE_CAPACITY_DEEP=0)
case "$variant" in
  specialists) flags=() ;;
  wide160|wide160-fullkey) flags+=(-DFORGE_CAPACITY_P=160) ;;
  deep32) flags=(-DFORGE_CAPACITY_P=128 -DFORGE_CAPACITY_DEEP=1) ;;
  wide192|wide192-syzygy|repetition-guard) flags+=(-DFORGE_CAPACITY_P=192 -DFORGE_HEAD_WIDTH=32 -DFORGE_HEADS=4) ;;
  reader64) flags+=(-DFORGE_CAPACITY_P=192 -DFORGE_HEAD_WIDTH=64 -DFORGE_HEADS=2 -DFORGE_COST_MASK4=0) ;;
  *) printf 'Unknown variant: %s\n' "$variant" >&2; exit 2 ;;
esac
src="$root/variants/$variant/src"
out="$root/bin/$variant"
mkdir -p -- "$root/bin"
sources=(bench bitboard endgame eval main movegen movepick pawns position search tests timeman tt uci forge_next_probe)
args=()
for source in "${sources[@]}"; do args+=("$src/$source.cpp"); done
"${CXX:-g++}" -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -flto=1 \
  "${flags[@]}" -DFORGE_NEXT_PROBE -DFORGE_NEXT_OPT=2 -DFORGE_CACHE_AUDIT \
  -march=haswell -mtune=generic -frandom-seed=forge2-placement-policy \
  "-ffile-prefix-map=$root=." -pthread -iquote "$src" \
  "${args[@]}" "$src/syzygy/tbprobe.cpp" -o "$out.tmp"
mv -- "$out.tmp" "$out"
printf '%s\n' "$out"
