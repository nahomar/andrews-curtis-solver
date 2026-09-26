#!/bin/zsh
# Restart every background job after a reboot / shutdown. Safe to run twice (skips what is already running).
cd ~/acc/acsolver
running() { pgrep -f "$1" >/dev/null; }
running "caffeinate -dimsu"  || nohup caffeinate -dimsu >/dev/null 2>&1 &
running "serve_guide.py"     || (cd ~/acc/learn && nohup ~/acc/vbias/.venv/bin/python serve_guide.py guide0i.pt /tmp/acs_guide.sock > ~/acc/acsolver/results/guide_server.log 2>&1 &)
running "power_guard.sh"     || nohup ./power_guard.sh >/dev/null 2>&1 &
running "cloudsync.sh"       || nohup ./cloudsync.sh >/dev/null 2>&1 &
running "sleep 7200; ./autosubmit.sh" || nohup zsh -c 'while true; do sleep 7200; ./autosubmit.sh; done' > results/autosubmit_loop.out 2>&1 &
if ! running "focus_loop.sh"; then
  N=$(( $(grep -o 'round [0-9]*' results/focus.log | tail -1 | awk '{print $2}') + 1 ))
  nohup ./focus_loop.sh $N > results/focus_loop.out 2>&1 &
fi
./autosubmit.sh                     # bank anything the cloud found while the Mac was down
echo "all jobs running"
