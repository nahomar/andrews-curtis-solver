#!/bin/zsh
# Event stream for continuous supervision: one line per actionable event.
cd ~/acc/acsolver
tail -n0 -F results/focus.log results/autosubmit.log 2>/dev/null | grep --line-buffered -E "submission [0-9]+:|FAILED|rejected [1-9]" | sed -u 's/^/EVENT /' &
last_rank=""; t=0
while true; do
  now=$(date +%s)
  [ -z "$(pgrep -x acs)" ] && { sleep 120; [ -z "$(pgrep -x acs)" ] && echo "ALERT no solver running on the Mac for 2+ minutes"; }
  b=$(pmset -g batt); p=$(print -r -- "$b" | grep -o "[0-9]*%" | head -1 | tr -d "%")
  last_b=$(cat results/.batt_last 2>/dev/null || echo 100)
  if print -r -- "$b" | grep -q "AC Power"; then echo 100 > results/.batt_last; else
    bk=100; for th in 20 30 40 45; do [ "${p:-100}" -le $th ] && { bk=$th; break; }; done
    [ $bk -lt ${last_b:-100} ] && { echo "ALERT Mac battery at ${p}% (solvers pause below 40%)"; echo $bk > results/.batt_last; }; fi
  ls=$(stat -f %m results/cloudsync.log 2>/dev/null || echo 0)
  [ $((now - ls)) -gt 2700 ] && [ ! -f results/.sync_alerted ] && { echo "ALERT cloud sync has not run for $(( (now - ls) / 60 )) minutes"; touch results/.sync_alerted; }
  [ $((now - ls)) -le 2700 ] && rm -f results/.sync_alerted
  lastf=results/.rank_checked; lm=$(stat -f %m $lastf 2>/dev/null || echo 0)
  if [ $((now - lm)) -ge 3600 ]; then
    touch $lastf
    r=$(python3 - <<'PY' 2>/dev/null
import json, sys
sys.argv = ['x']
exec(open('sair.py').read().split("cmd = sys.argv[1]")[0])
out = []
for p in ('ac', 'stable_ac'):
    items, cur = [], None
    while True:
        code, d, _ = call(f'/leaderboard?problem={p}' + (f'&cursor={cur}' if cur else ''))
        items += d['data']['items']; cur = d['data'].get('nextCursor')
        if not cur: break
    m = next(x for x in items if x['team'].get('teamNumber') == 'ACC01-T00129')
    out.append(f"{p} #{m['rank']} {float(m['score']):.1f}")
print(' | '.join(out))
PY
)
    prev=$(cat results/.rank_last 2>/dev/null)
    [ -n "$r" ] && [ "$r" != "$prev" ] && { echo "RANK $r"; print -r -- "$r" > results/.rank_last; }
  fi
  t=$((t + 1)); sleep 60
done
