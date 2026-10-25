#!/bin/zsh
# One symmetry-splice pass over everything on disk (DBs + all solver outputs incl. the cloud's),
# then merge via the official verifier. Cheap (~1 min, ~2 GB) -> run after every cloud sync / focus round.
cd ~/acc/acsolver
T=$(date +%m%d%H%M)
python3 orbit_pool.py --db best_ac.json best_sac.json --alternatives results/*_ac.jsonl results/*_sac.jsonl --out results/pool_$T.jsonl > /dev/null 2>&1 || exit 1
./orbit_splice --problems problems_ac.txt --paths results/pool_$T.jsonl --two-move-cap 64 --join-rounds 1 --out results/orbitr_$T > results/orbit_round.log 2>&1
python3 check.py results/orbitr_${T}_ac.jsonl --db best_ac.json 2>&1 | tail -1
python3 check.py results/orbitr_${T}_sac.jsonl --db best_sac.json 2>&1 | tail -1
rm -f results/pool_$T.jsonl
