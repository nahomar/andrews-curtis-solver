#!/bin/sh
# One run per untried fix, then all together. Each is ~25-70 min on an M-series GPU; results in runs/<tag>/eval.json.
set -e
PY=.venv/bin/python
$PY train.py --tag step      --w-step 1
$PY train.py --tag groww     --grow-w 4
$PY train.py --tag rankball  --w-rank-ball 1
$PY train.py --tag rankpath  --w-rank-path 1
$PY train.py --tag residual  --residual
$PY train.py --tag all       --w-step 1 --grow-w 2 --w-rank-ball 1 --w-rank-path 0.5 --residual
$PY compare.py
