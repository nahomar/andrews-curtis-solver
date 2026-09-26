#!/usr/bin/env python3
"""Cross-check of grp.cpp's TRIVIAL verdicts with SymPy's independent coset enumeration.
For each presentation <x,y|u,v> given as lines "t u v ...", compute the index of <x> with SymPy;
index 1 together with trivial abelianisation (det = +-1) means the group is trivial."""
import sys
from sympy.combinatorics.free_groups import free_group
from sympy.combinatorics.fp_groups import FpGroup
F, x, y = free_group("x, y")
L = {"x": x, "X": x**-1, "y": y, "Y": y**-1}
def word(s):
    w = F.identity
    for c in s: w = w * L[c]
    return w
for line in sys.stdin:
    p = line.split()
    if len(p) < 3: continue
    G = FpGroup(F, [word(p[1]), word(p[2])])
    C = G.coset_enumeration([x], max_cosets=2000000)
    C.compress(); C.standardize()
    print(p[0], p[1], p[2], "index_of_<x> =", len(C.table), flush=True)
