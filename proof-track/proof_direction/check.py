#!/usr/bin/env python3
"""Independent checker for the length-<=L classification (pure Python, standard library only).

It re-uses NOTHING from acenum.cpp / grp.cpp except their output files.  AC moves are replayed with
the OFFICIAL SAIR verifier (tools/verifier/core.py, move spec ac-r2-v1).

  python3 check.py --cert cert_*.txt --groups groups.txt --len L

Checks
  (1) every EDGE line "EDGE nu nv pu pv h swp m1 m2 ...": replay the official move ids m_i on the exact state
      (pu, pv) with verifier.core.apply_move, apply the signed letter permutation h, (swap if swp) and compare
      EXACTLY with (nu, nv).  Presentations connected by edges to ROOT (x, y) are AC-trivial; presentations
      connected to a SEED are AC-equivalent to that seed up to a signed letter permutation (README, Lemmas).
  (2) every NONTRIVIAL line: the two permutations satisfy both relators and are not both the identity
      => the presented group has a nontrivial finite quotient, so it is not the trivial group.
  (3) completeness: independently enumerate all balanced presentations <x,y|u,v> with u, v cyclically reduced,
      |u|+|v| <= L and exponent-sum determinant +-1, up to rotation/inversion of relators, swapping relators
      and signed letter permutations (this checker's own canonical form), and check that every class is
      certified AC-trivial, certified nontrivial, or listed as residual.
"""
import argparse, os, sys, time
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.expanduser("~/acc/official/competition/tools"))
from verifier.core import apply_move, free_reduce  # official ac-r2-v1 semantics

LET = {"x": 1, "X": -1, "y": 2, "Y": -2}


def parse(s):
    w = tuple(LET[c] for c in s)
    assert free_reduce(w) == w, s
    return w


def inv(w):
    return tuple(-a for a in reversed(w))


def hmap(w, h):
    """Signed letter permutation: bit 1 swaps x<->y, then bit 2 inverts x, bit 4 inverts y."""
    out = []
    for a in w:
        g, s = abs(a), (1 if a > 0 else -1)
        if h & 1:
            g = 3 - g
        if (g == 1 and h & 2) or (g == 2 and h & 4):
            s = -s
        out.append(s * g)
    return tuple(out)


def is_cyc_reduced(w):
    return len(w) > 0 and free_reduce(w) == w and (len(w) == 1 or w[0] != -w[-1])


def cword(w):
    """This checker's canonical form of a cyclic word up to inversion (numeric tuple order)."""
    n = len(w)
    best = None
    for x in (w, inv(w)):
        for i in range(n):
            r = x[i:] + x[:i]
            if best is None or r < best:
                best = r
    return best


def cpair(u, v):
    best = None
    for h in range(8):
        a, b = cword(hmap(u, h)), cword(hmap(v, h))
        k = (min((len(a), a), (len(b), b)), max((len(a), a), (len(b), b)))
        if best is None or k < best:
            best = k
    return best


def expo(w):
    return sum(1 for a in w if a == 1) - sum(1 for a in w if a == -1), sum(1 for a in w if a == 2) - sum(1 for a in w if a == -2)


# ---------------------------------------------------------------- (1) certificate
class UF:
    def __init__(self):
        self.p = {}

    def find(self, a):
        self.p.setdefault(a, a)
        r = a
        while self.p[r] != r:
            r = self.p[r]
        while self.p[a] != r:
            self.p[a], a = r, self.p[a]
        return r

    def union(self, a, b):
        self.p[self.find(a)] = self.find(b)


def check_cert(paths):
    """Replays every EDGE with the official verifier.  EDGE nu nv pu pv h swp moves: the official moves take
    the exact state (pu, pv) to a state Q, and (nu, nv) = h(Q) (relators swapped if swp).  Hence
    (nu, nv) ~AC h((pu, pv)).  Components of the resulting graph therefore consist of presentations that are
    pairwise AC-equivalent up to a signed letter permutation; the component of ROOT = (x, y) consists of
    AC-trivial presentations (AC-triviality is invariant under signed letter permutations)."""
    uf = UF()
    root, seeds, n_edges = None, [], 0
    t0 = time.time()
    for path in paths:
        with open(path) as f:
            for line in f:
                p = line.split()
                if not p:
                    continue
                if p[0] == "ROOT":
                    st = (parse(p[1]), parse(p[2]))
                    assert st == ((1,), (2,)), "root must be the standard presentation (x, y)"
                    root = st
                    uf.find(st)
                    continue
                if p[0] == "SEED":
                    st = (parse(p[1]), parse(p[2]))
                    seeds.append((p[1], p[2], p[3] if len(p) > 3 else "%s %s" % (p[1], p[2])))
                    uf.find(st)
                    continue
                assert p[0] == "EDGE", line
                u, v, pu, pv = map(parse, p[1:5])
                h, swp = int(p[5]), int(p[6])
                assert 0 <= h < 8 and swp in (0, 1)
                st = (pu, pv)
                for m in p[7:]:
                    m = int(m)
                    assert 0 <= m < 14
                    st = apply_move(st, m)
                img = (hmap(st[0], h), hmap(st[1], h))
                if swp:
                    img = (img[1], img[0])
                assert img == (u, v), "edge replay mismatch for %s %s" % (p[1], p[2])
                uf.union((u, v), (pu, pv))
                n_edges += 1
    assert root is not None
    r = uf.find(root)
    triv = set()
    comp = {}          # checker-canonical class -> seed label
    for st in list(uf.p):
        c = uf.find(st)
        if c == r:
            triv.add(cpair(*st))
    seed_of = {}
    for (a, b, name) in seeds:
        seed_of.setdefault(uf.find((parse(a), parse(b))), name)
    for st in list(uf.p):
        c = uf.find(st)
        if c != r and c in seed_of:
            comp.setdefault(cpair(*st), seed_of[c])
    merged = [s for s in seeds if uf.find((parse(s[0]), parse(s[1]))) == r]
    print("(1) certificate: %d edges replayed with the official verifier; %d AC-trivial classes; %d seeds (%d of them shown AC-trivial) (%.0fs)"
          % (n_edges, len(triv), len(seeds), len(merged), time.time() - t0))
    return triv, comp


# ---------------------------------------------------------------- (2) nontriviality certificates
def act(perm_x, perm_y, w, pt):
    ix = [0] * len(perm_x)
    iy = [0] * len(perm_y)
    for i, j in enumerate(perm_x):
        ix[j] = i
    for i, j in enumerate(perm_y):
        iy[j] = i
    for a in w:
        pt = {1: perm_x, -1: ix, 2: perm_y, -2: iy}[a][pt]
    return pt


def check_groups(path):
    nontriv, resid = set(), {}
    with open(path) as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            u, v = parse(p[1]), parse(p[2])
            if p[3] == "NONTRIVIAL":
                px, py = eval(p[5]), eval(p[6])   # lists of ints
                n = len(px)
                assert sorted(px) == list(range(n)) and sorted(py) == list(range(n))
                assert px != list(range(n)) or py != list(range(n))
                for w in (u, v):
                    assert all(act(px, py, w, i) == i for i in range(n)), "relator not satisfied"
                nontriv.add(cpair(u, v))
            else:
                resid[cpair(u, v)] = (p[1], p[2], p[3])
    print("(2) %d nontriviality certificates verified (homomorphisms onto nontrivial permutation groups)" % len(nontriv))
    return nontriv, resid


# ---------------------------------------------------------------- (3) completeness
def necklaces(n):
    """All cyclically reduced words of length n equal to their own canonical form, grouped by exponent vector."""
    out = defaultdict(list)
    letters = (1, -1, 2, -2)

    def rec(w):
        if len(w) == n:
            if (n == 1 or w[0] != -w[-1]) and cword(w) == w:
                out[expo(w)].append(w)
            return
        for a in letters:
            if w and w[-1] == -a:
                continue
            rec(w + (a,))

    rec(())
    return out


RESMAP = []


def completeness(L, triv, nontriv, resid, comp):
    t0 = time.time()
    neck = {n: necklaces(n) for n in range(1, L)}
    print("    necklaces generated (%.0fs)" % (time.time() - t0))
    total = defaultdict(set)
    for t in range(2, L + 1):
        for a in range(1, t // 2 + 1):
            b = t - a
            for (p, q), us in neck[a].items():
                for (ex, ey), vs in neck[b].items():
                    if p * ey - q * ex not in (1, -1):
                        continue
                    for u in us:
                        for v in vs:
                            total[t].add(cpair(u, v))
    ok = True
    for t in range(2, L + 1):
        cl = total[t]
        nt = sum(1 for k in cl if k in triv)
        nn = sum(1 for k in cl if k not in triv and k in nontriv)
        nr = sum(1 for k in cl if k not in triv and k not in nontriv and k in resid)
        miss = len(cl) - nt - nn - nr
        res_seeds = defaultdict(int)
        for k in cl:
            if k not in triv and k not in nontriv and k in resid:
                res_seeds[comp.get(k, "UNLINKED")] += 1
        print("    length %2d: %6d classes = %6d AC-trivial + %4d nontrivial group + %4d residual in %d certified residual components  (unaccounted: %d)"
              % (t, len(cl), nt, nn, nr, len(res_seeds), miss))
        named = ", ".join("%s:%d" % (sd, c) for sd, c in sorted(res_seeds.items(), key=lambda z: -z[1]) if not sd.startswith("R"))
        if named:
            print("               residual classes in the components of the named presentations: " + named)
        for k in cl:
            if k not in triv and k not in nontriv and k in resid:
                RESMAP.append((t, resid[k][0], resid[k][1], comp.get(k, "UNLINKED")))
        ok &= miss == 0
    print("(3) completeness up to length %d: %s (%.0fs)" % (L, "OK" if ok else "FAILED", time.time() - t0))
    return ok


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--cert", required=True, nargs="+")
    ap.add_argument("--groups", required=True)
    ap.add_argument("--len", type=int, required=True)
    ap.add_argument("--resmap", default=None, help="write residual class -> component map here")
    a = ap.parse_args()
    triv, comp = check_cert(a.cert)
    nontriv, resid = check_groups(a.groups)
    ok = completeness(a.len, triv, nontriv, resid, comp)
    if a.resmap:
        with open(a.resmap, "w") as f:
            for r in RESMAP:
                f.write("%d %s %s %s\n" % r)
    sys.exit(0 if ok else 1)
