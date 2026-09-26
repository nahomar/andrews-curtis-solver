#!/bin/zsh
# Wait for the Mac sweep (by PID), then run campaign rounds.
cd ~/acc/acsolver
while kill -0 ${1:-72825} 2>/dev/null; do sleep 60; done
./campaign.sh 1 5000
./campaign.sh 2 8000
./campaign.sh 3 12000
