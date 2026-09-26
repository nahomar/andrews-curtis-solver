#!/bin/zsh
# Round N: merge -> near-miss reruns -> splice -> merge -> SAC -> submission plan.
#   ./campaign.sh N [WIDTH] [SEED]     (expects results/*.jsonl from earlier rounds)
set -e
N=${1:-1}; W=${2:-5000}; SEED=${3:-$((100 + N))}; G=${GMAX:-3}; RMAX=${RMAX:-100000}; EXTRA=(${=EXTRA:-})
R=r$(date +%m%d%H%M)_$N          # unique per run: output files must never collide (the solver resumes by id)
T=${THREADS:-10}
log() { print -r -- "[$(date '+%m-%d %H:%M')] $*" | tee -a results/campaign.log; }
log "round $N start (width $W seed $SEED gap<=$G record<$RMAX extra: $EXTRA)"
python3 check.py results/*_ac.jsonl --db best_ac.json | tee -a results/campaign.log
python3 tools.py near-misses $G results/nm_${R}.txt $RMAX | tee -a results/campaign.log
grep '^ac-' results/nm_${R}.txt > results/nm_${R}_ac.txt || true
./acs solve --problems results/nm_${R}_ac.txt --ball ${BALL:-ball17.bin} --width $W --threads $T --time-limit 240 \
    --radius 4 --look 3 --power 8 --seed $SEED $EXTRA --out results/nm_${R}_ac.jsonl 2>> results/campaign.err
ALL=$(ls results/*_ac.jsonl | grep -v splice | paste -sd, -)
./acs splice --problems results/nm_${R}_ac.txt --paths $ALL --ball ${BALL:-ball17.bin} --radius 3 --threads $T \
    --out results/splice_${R}_ac.jsonl 2>> results/campaign.err
python3 check.py results/*_ac.jsonl --db best_ac.json | tee -a results/campaign.log
# restart from waypoints of the best known paths, then splice (rebeam)
python3 -c "import json; db=json.load(open('best_ac.json')); ids={l.split()[0] for l in open('results/nm_${R}_ac.txt')}; f=open('results/rbin_${R}.jsonl','w'); [f.write(json.dumps({'id':i,'solved':True,'length':db[i]['length'],'moves':db[i]['moves']})+'\\n') for i in ids if i in db]"
./acs rebeam --problems results/nm_${R}_ac.txt --paths results/rbin_${R}.jsonl --ball ${BALL:-ball17.bin} --width 3000 --threads $T --time-limit 60 \
    --look 3 --radius 3 --seed $SEED --out results/rebeam_${R}_ac.jsonl 2>> results/campaign.err
python3 check.py results/*_ac.jsonl --db best_ac.json | tee -a results/campaign.log
python3 tools.py sac-from-ac | tee -a results/campaign.log
python3 -c "import json; ids={json.loads(l)['id'] for l in open('results/sac_from_ac.jsonl')}; open('results/sac_ids.txt','w').writelines(l for l in open('problems_sac.txt') if l.split()[0] in ids)"
./acs shorten --stable --problems results/sac_ids.txt --paths results/sac_from_ac.jsonl --ball ball16s.bin --radius 3 --threads $T \
    --out results/sacshort_${R}_sac.jsonl 2>> results/campaign.err
python3 check.py results/sac_from_ac.jsonl results/*_sac.jsonl(N) --db best_sac.json | tee -a results/campaign.log
python3 tools.py plan submit | tee -a results/campaign.log
log "round $N done"
