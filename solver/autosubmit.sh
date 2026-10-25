#!/bin/zsh
# One auto-submit round: refresh records, merge, SAC, plan, verify, submit new improvements, update ledger.
cd ~/acc/acsolver
mkdir .autosubmit.lock 2>/dev/null || { echo "another round is running"; exit 0; }   # one round at a time
trap "rmdir .autosubmit.lock" EXIT
log() { print -r -- "[$(date '+%m-%d %H:%M')] $*" >> results/autosubmit.log; }
log "round start"
python3 sair.py snapshot >> results/autosubmit.log 2>&1 || log "snapshot failed (read quota?) - using cached records.json"
python3 check.py $(ls results/*.jsonl | grep -v -E "train|_sac\.jsonl|sac_from_ac") --db best_ac.json >> results/autosubmit.log 2>&1
python3 tools.py sac-from-ac >> results/autosubmit.log
python3 check.py results/sac_from_ac.jsonl $(ls results/*_sac.jsonl 2>/dev/null) --db best_sac.json >> results/autosubmit.log 2>&1
rm -rf submit && pl=$(python3 tools.py plan submit) && print -r -- "$pl" >> results/autosubmit.log
# quota guard: 40 files/day are shared with the other agent. Defer tiny batches until they accumulate
# (they are re-planned from the DB every round, nothing is lost) and cap this session's daily count.
total=$(print -r -- "$pl" | sed -n 's/.*submit_.*expected +\([0-9.]*\) points.*/\1/p' | awk '{s+=$1} END{printf "%.3f", s+0}')
day=$(date +%Y%m%d); cnt=$(cat results/.subcount_$day 2>/dev/null || echo 0)
# pace the 20 daily files over the day (4 up front, then 2 per 3 hours) so a valuable late batch is never locked out
pace=$(( 4 + 16 * $(date +%H | sed "s/^0//") / 24 )); [ $pace -gt ${DAILYCAP:-20} ] && pace=${DAILYCAP:-20}
[ "$cnt" -ge $pace ] && { log "pacing: $cnt files used, $pace allowed by this hour; deferring (expected +$total)"; exit 0; }
if [ $(date +%H) -lt 22 ] && [ $(print -r -- "$total < ${MINGAIN:-0.1}" | bc) -eq 1 ]; then log "deferring small batch (expected +$total < ${MINGAIN:-0.1})"; exit 0; fi
ls submit/submit_*.txt(N) >/dev/null 2>&1 && [ -n "$(ls submit/submit_*.txt 2>/dev/null)" ] || { log "nothing improves our score; no submission this round"; exit 0; }
n=0
for f in submit/submit_*.txt(N); do
  [ $n -ge ${MAXFILES:-8} ] && break
  F="$HOME/acc/acsolver/$f"
  (cd ~/acc/official && PYTHONPATH=competition/tools python3 -m verifier --manifest competition/tools/verifier/data/manifest.json --submission "$F") \
    | python3 -c "import json,sys; o=json.load(sys.stdin); r=o['results']; sys.exit(0 if o.get('accepted') and all(x.get('ok') for x in r) else 1)" \
    || { log "local verify FAILED for $f, skipped"; continue; }
  out=$(python3 sair.py submit $f --yes 2>&1); print -r -- "$out" >> results/autosubmit.log
  sid=$(print -r -- "$out" | sed -n 's/^submission id //p')
  [ -z "$sid" ] && { log "submit failed for $f"; continue; }
  python3 - "$sid" "$f" <<'PY' >> results/autosubmit.log 2>&1
import json, sys
sid, f = sys.argv[1], sys.argv[2]
led = json.load(open('submitted.json')); n = 0
for l in open(f):
    if l.strip() and not l.startswith('#'):
        i, mv = l.split(':', 1); L = len(json.loads(mv)); led[i.strip()] = min(led.get(i.strip(), 10**9), L); n += 1
json.dump(led, open('submitted.json', 'w'))
print(f'submission {sid}: accepted for processing, {n} locally verified lines recorded (no status polling, to save read quota)')
PY
  n=$((n+1)); echo $((cnt+n)) > results/.subcount_$day
done
log "round done: $n files submitted"
