#!/bin/bash
# Perpetual cloud worker: new seed + rotating settings every pass over targets.txt (refreshed by the Mac's sync).
# Every 3rd pass polishes our best paths (paths.jsonl, pushed by the Mac) with rebeam + window instead of solving from scratch.
cd ~/acs
W=${1:-6000}; BASE=${2:-30000}
SETS=("--junk 2 --look-pen 8" "--junk 8 --look-pen 32" "--junk 4 --look-pen 16 --power 0" "--junk 4 --look 0" "--junk 3 --look-pen 12 --reps 5" "--junk 4 --look-pen 16" "--junk 4 --look-pen 16 --lincap 60 --linper 8" "--junk 4 --look-pen 16 --lincap 20 --linper 4")
k=$(cat .forever_k 2>/dev/null || echo 0)
while true; do
  [ -s targets.txt ] || { sleep 60; continue; }
  S=${SETS[$((k % ${#SETS[@]}))]}
  if [ $((k % 3)) -eq 2 ] && [ -s paths.jsonl ]; then
    cp paths.jsonl paths_run.jsonl
    ./acs rebeam --problems targets.txt --paths paths_run.jsonl --ball ball16.bin --threads $(nproc) --width 4000 --time-limit 20 \
        --look 3 --radius 3 --seed $((BASE + k)) $S --out results/p5_${k}_rb_ac.jsonl >> results/forever.log 2>&1 < /dev/null
    ./acs window --problems targets.txt --paths paths_run.jsonl --threads $(nproc) --width 3000 \
        --out results/p5_${k}_win_ac.jsonl >> results/forever.log 2>&1 < /dev/null
  else
    ./acs solve --problems targets.txt --ball ball16.bin --threads $(nproc) --radius 4 --look 3 --width $W --time-limit 240 \
        --seed $((BASE + k)) $S --out results/f5_${k}_ac.jsonl >> results/forever.log 2>&1 < /dev/null
  fi
  k=$((k + 1)); echo $k > .forever_k
done
