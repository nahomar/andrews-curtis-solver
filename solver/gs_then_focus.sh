#!/bin/zsh
# Solve never-solved puzzles with the distance-map oracle (new near-miss material), then resume the focus loop.
cd ~/acc/acsolver
./acs solve --problems results/nopath100_rest.txt --ball ball17.bin --gs gs22.bin --width 3000 --threads 12 --time-limit 240 \
    --radius 4 --look 3 --seed 14501 --out results/gsnop2_ac.jsonl > /dev/null 2>> results/gsnop2.err
./autosubmit.sh
N=$(( $(grep -o 'round [0-9]*' results/focus.log | tail -1 | awk '{print $2}') + 1 ))
nohup ./focus_loop.sh $N > results/focus_loop.out 2>&1 &
