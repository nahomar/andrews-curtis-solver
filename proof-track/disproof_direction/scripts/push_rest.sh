#!/bin/sh
# Frontier BFS at cap 25 for the Sigma-classes whose hash-table run stopped at cap 24, then cap 26 for the
# two MS(2) length-14 blocks.  2 threads.
cd "$(dirname "$0")/.."
run() { name=$1; L=$2; shift 2
  [ -f data/fcc_${name}_cap$L.log ] && grep -q RESULT data/fcc_${name}_cap$L.log && return
  /usr/bin/time -l scripts/fcc --quot --cap $L --threads 2 --watch data/watch.txt -- "$@" > data/fcc_${name}_cap$L.log 2>&1; }
for id in ms0060 ms0088 ms0116 ms0117 ms0551; do
  words=$(grep "^$id " data/open_ms.txt | cut -d'#' -f1 | cut -d' ' -f2-)
  run $id 25 $words
done
run ms0352 26 -1 2 2 1 -2 -2 -2 '|' 1 2 -1 -1 2 1 1
run ms0359 26 -1 2 2 1 -2 -2 -2 '|' 1 -2 -1 -1 2 1 1
