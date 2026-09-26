#!/bin/sh
# Reproduce the length <= 16 classification (single-threaded; ~1.5 GB RAM peak for the cap-22 BFS and ~1 h total).
# Quick check only (uses shipped certificates, ~2 min):   sh reproduce.sh check
set -e
cd "$(dirname "$0")"
if [ "$1" = "check" ]; then
  python3 check.py --cert certificates/cert_triv16.txt certificates/cert_hits16_c28.txt certificates/cert_residual16_c28.txt \
                   --groups certificates/groups16.txt --len 16 --resmap work/resmap16.txt
  exit 0
fi
mkdir -p work
clang++ -O3 -std=c++17 -o acenum acenum.cpp
clang++ -O3 -std=c++17 -o work/grp grp.cpp
# 1. component of (x,y) in the GS graph with total length <= 22 (21,194,887 classes, ~9 min)
./acenum bfs --cap 22 --out work/comp22.bin
# 2. all det=+-1 classes up to length 17, flagged if in the component
./acenum class --len 17 --comp work/comp22.bin --out data/classes17_c22.txt
# 3. group-theoretic triage of the rest (Todd-Coxeter over cyclic subgroups; permutation quotients)
awk '$4==0' data/classes17_c22.txt | work/grp 2000000 8 > work/groups17_c22.txt
awk '$1<=16' work/groups17_c22.txt > certificates/groups16.txt
awk '$1<=16 && $4!="NONTRIVIAL"' work/groups17_c22.txt > work/res16_trivgrp.txt
# 4. certificates: AC-trivial classes (tree edges of the cap-22 component)
awk '$1<=16 && $4==1{print $2,$3}' data/classes17_c22.txt > work/need16.txt
./acenum cert --comp work/comp22.bin --need work/need16.txt --out certificates/cert_triv16.txt
# 5. residual components at cap 28, seeded by the named presentations (data/seeds.txt)
./acenum resolve --cap 28 --comp work/comp22.bin --start data/seeds.txt --need work/res16_trivgrp.txt --out certificates/cert_residual16_c28.txt
mv certificates/cert_residual16_c28.txt.summary data/residual_components16_c28.txt
./acenum cert --comp work/comp22.bin --need certificates/cert_residual16_c28.txt.hits --out certificates/cert_hits16_c28.txt
mv certificates/cert_residual16_c28.txt.hits work/
# 6. independent check
python3 check.py --cert certificates/cert_triv16.txt certificates/cert_hits16_c28.txt certificates/cert_residual16_c28.txt \
                 --groups certificates/groups16.txt --len 16 --resmap data/residual_classes16.txt
