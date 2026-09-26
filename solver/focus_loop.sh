#!/bin/zsh
# Near-miss conversion loop: every round = fresh seed + rotating settings on the focus list, rebeam from the best
# paths, splice everything (Mac + cloud results), SAC shortening, then submit right away if anything raises our score.
cd ~/acc/acsolver
N=${1:-100}
SETTINGS=("--junk 4 --look-pen 16" "--gs gs22.bin --gsw 2 --gspen 16" "--junk 2 --look-pen 8" "--junk 8 --look-pen 32" "--gs gsw22.bin --gsw 4 --gspen 16" "--junk 4 --look-pen 16 --power 0" "--junk 4 --look 0" "--junk 3 --look-pen 12 --reps 5")
log() { print -r -- "[$(date '+%m-%d %H:%M')] $*" >> results/focus.log; }
GMAX=6
while true; do
  for S in $SETTINGS; do
    R=f$(date +%m%d%H%M)_$N
    python3 check.py results/*_ac.jsonl --db best_ac.json >> results/focus.log 2>&1
    FOCUS_GMAX=$GMAX python3 tools.py focus results/focus_${R}.txt >> results/focus.log
    log "round $N ($R) settings: $S"
    ./acs solve --problems results/focus_${R}.txt --ball ball17.bin --width 12000 --threads 12 --time-limit 180 \
        --radius 4 --look 3 --power 8 --seed $((5000 + N)) ${=S} --out results/fsolve_${R}_ac.jsonl 2>> results/focus.err
    python3 check.py results/*_ac.jsonl --db best_ac.json >> results/focus.log 2>&1
    python3 -c "import json; db=json.load(open('best_ac.json')); ids=[l.split()[0] for l in open('results/focus_${R}.txt')]; f=open('results/frb_${R}.jsonl','w'); [f.write(json.dumps({'id':i,'solved':True,'length':db[i]['length'],'moves':db[i]['moves']})+'\n') for i in ids if i in db]"
    head -120 results/focus_${R}.txt > results/focusrb_${R}.txt   # rebeam only the top targets (few holders first)
    ./acs rebeam --problems results/focusrb_${R}.txt --paths results/frb_${R}.jsonl --ball ball17.bin --width 4000 --threads 12 \
        --time-limit 20 --look 3 --radius 3 --seed $((7000 + N)) ${=S} --out results/frebeam_${R}_ac.jsonl 2>> results/focus.err
    ALL=$(ls results/*_ac.jsonl | grep -v -E "splice" | paste -sd, -)
    ./acs splice --problems results/focus_${R}.txt --paths $ALL --ball ball17.bin --radius 3 --threads 12 --out results/fsplice_${R}_ac.jsonl 2>> results/focus.err
    ./autosubmit.sh
    last=$(tail -1 results/autosubmit.log)
    log "round $N done (window <=$GMAX): $(grep -c . results/fsolve_${R}_ac.jsonl) solved rows; $last"
    if print -r -- "$last" | grep -q "files submitted"; then GMAX=6; else GMAX=$(( GMAX < 12 ? GMAX + 2 : 12 )); fi
    N=$((N + 1))
  done
done
