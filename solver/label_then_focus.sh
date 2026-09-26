#!/bin/zsh
# Use the Mac for RL labelling while the SAIR read quota is exhausted, then resume the focus loop after the reset.
cd ~/acc/acsolver
./acs solve --problems ~/acc/learn/states3.txt --ball ball17.bin --width 1000 --threads 12 --time-limit 15 --radius 3 --seed 31 --junk 4 --look-pen 16 --out ~/acc/learn/lab3a.jsonl > /dev/null 2>> results/label3.err
./acs solve --problems ~/acc/learn/states3.txt --ball ball17.bin --width 1000 --threads 12 --time-limit 15 --radius 3 --seed 32 --junk 2 --look-pen 8 --out ~/acc/learn/lab3b.jsonl > /dev/null 2>> results/label3.err &
LP=$!
while [ $(date +%H%M) -lt 1705 ]; do sleep 60; done       # SAIR read quota resets at 17:00 local
kill $LP 2>/dev/null
N=$(( $(grep -o 'round [0-9]*' results/focus.log | tail -1 | awk '{print $2}') + 1 ))
./autosubmit.sh
nohup ./focus_loop.sh $N > results/focus_loop.out 2>&1 &
