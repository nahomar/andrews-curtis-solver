#!/bin/zsh
# After the current direct Stable-AC run, keep 2 cores on direct Stable-AC search over wider SAC near-miss lists.
cd ~/acc/acsolver
while pgrep -f "problems results/sac_direct.txt" >/dev/null; do sleep 60; done
N=2
while true; do
  python3 - $N <<'PY'
import json, sys
rec = json.load(open('records.json')); sac = json.load(open('best_sac.json'))
P = {l.split()[0]: l for l in open('problems_sac.txt')}
rows = sorted(((0 if rec['ac-' + i.split('-')[1]][3] <= 2 else 1, v['length'] - rec['ac-' + i.split('-')[1]][2], i)
               for i, v in sac.items() if rec['ac-' + i.split('-')[1]][2] and 1 <= v['length'] - rec['ac-' + i.split('-')[1]][2] <= 8))
open(f'results/sacchain_{sys.argv[1]}.txt', 'w').writelines(P[i] for _, _, i in rows); print(len(rows))
PY
  ./acs solve --stable --problems results/sacchain_${N}.txt --ball ball16s.bin --width 6000 --threads 2 --time-limit 180 \
      --radius 4 --look 3 --seed $((9100 + N)) --out results/sacchain${N}_sac.jsonl > /dev/null 2>> results/sacchain.err
  N=$((N + 1))
done
