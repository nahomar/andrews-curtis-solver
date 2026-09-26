#!/bin/sh
# Push the three length-14 Sigma-classes (AK(3); MS(2) blocks of ms0352 and ms0359) to higher caps with the
# frontier BFS (no paths; watch hits only).  2 threads.  Logs: data/fcc_<name>_cap<L>.log
cd "$(dirname "$0")/.."
AK3="1 1 1 -2 -2 -2 -2 | 1 2 1 -2 -1 -2"
M352="-1 2 2 1 -2 -2 -2 | 1 2 -1 -1 2 1 1"
M359="-1 2 2 1 -2 -2 -2 | 1 -2 -1 -1 2 1 1"
run() { name=$1; L=$2; shift 2
  [ -f data/fcc_${name}_cap$L.log ] && grep -q RESULT data/fcc_${name}_cap$L.log && return
  /usr/bin/time -l scripts/fcc --quot --cap $L --threads 2 --watch data/watch.txt -- "$@" > data/fcc_${name}_cap$L.log 2>&1; }
for L in ${CAPS:-25 26}; do
  run ak3 $L $AK3
  run ms0352 $L $M352
  run ms0359 $L $M359
done
