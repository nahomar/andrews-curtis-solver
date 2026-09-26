#!/bin/zsh
# Wait for round-3 labels, give them their own id namespace, then train on all deep-label rounds.
cd ~/acc/learn
until [ -f lab3b.jsonl ] && [ $(wc -l < lab3b.jsonl) -ge 36000 ]; do sleep 120; done
sed 's/"lab-/"l3-/' meta3.jsonl > meta3n.jsonl
sed 's/"lab-/"l3-/' lab3a.jsonl > lab3an.jsonl; sed 's/"lab-/"l3-/' lab3b.jsonl > lab3bn.jsonl
cat meta_16000.jsonl meta3n.jsonl > meta_all.jsonl
~/acc/vbias/.venv/bin/python train_guide.py --labels lab2a_acc1.jsonl lab2b_acc1.jsonl lab2a_acc2.jsonl lab2b_acc2.jsonl lab3an.jsonl lab3bn.jsonl \
    --meta meta_all.jsonl --steps 4000 --init ~/acc/vbias/runs/all/model.pt --out guide3.pt 2>&1 | grep -v -i warn
