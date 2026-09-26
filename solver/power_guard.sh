#!/bin/zsh
# Keep solving on AC power or while the battery is above 40%; pause below 40% on battery; resume when power returns.
while true; do
  b=$(pmset -g batt)
  pct=$(print -r -- "$b" | grep -o '[0-9]*%' | head -1 | tr -d '%')
  if print -r -- "$b" | grep -q "AC Power" || [ "${pct:-0}" -gt 40 ]; then
    for p in $(pgrep -x acs); do kill -CONT $p; done
  else
    for p in $(pgrep -x acs); do kill -STOP $p; done
  fi
  sleep 60
done
