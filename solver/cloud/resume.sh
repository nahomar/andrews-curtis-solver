#!/bin/bash
cd ~/acs
pgrep -f run_forever.sh >/dev/null && { echo running; exit 0; }
W=$( [ "$(nproc)" -ge 8 ] && echo 6000 || echo 3000 ); B=$( [ "$(nproc)" -ge 8 ] && echo 30000 || echo 40000 )
setsid nohup ./run_forever.sh $W $B > /dev/null 2>&1 < /dev/null & sleep 1; echo started
