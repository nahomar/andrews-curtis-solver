#!/bin/bash
cd ~/acs
[ -f cloud_paths.jsonl ] && mv cloud_paths.jsonl paths.jsonl
[ -f cloud_targets.txt ] && mv cloud_targets.txt targets.txt
chmod +x run_forever.sh
pkill -x acs; sleep 1
bash resume.sh
sleep 3
echo "paths $(wc -l < paths.jsonl) targets $(wc -l < targets.txt) k=$(cat .forever_k)"
ps -eo args | grep -E "^(\./acs|/bin/bash \./run_forever)" | cut -c1-60
