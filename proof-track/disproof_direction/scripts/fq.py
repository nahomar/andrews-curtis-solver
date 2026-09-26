"""Finite-quotient AC-orbit invariant (negative-result experiment, part (c)).

For a finite group G and an epimorphism phi: F2 -> G (a generating pair (g, h) = (phi(x), phi(y))), every
official AC move on a presentation P = (r0, r1) induces an "AC_G move" on the pair phi(P) in G x G:
    (a, b) -> (a^-1, b), (a b^+-1, b), (a, b a^+-1), (a^c, b), (a, b^c)   (c in G; phi is onto).
Hence  P ~AC Q  =>  phi(P) and phi(Q) lie in the same AC_G-orbit, for every phi.  Since phi(x, y) = (g, h),
an AC-trivial P must satisfy:  phi(P) ~ (g, h) in the AC_G graph, for ALL generating pairs (g, h).
A single failure would PROVE P is not AC-trivial.

Borovik-Lubotzky-Myasnikov (arXiv:1103.1295) describe the components of AC graphs of finite groups and show
that finite computations of this kind cannot produce a counterexample; this script is an explicit sanity
check of that negative result on the open Miller-Schupp instances (and AK(3)), with known-trivial controls.

Usage: python3 scripts/fq.py  -> data/fq_report.json
"""
import itertools, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ms

def mul(p, q):  # apply p then q  (left-to-right composition, words read left to right)
    return tuple(q[i] for i in p)

def inv(p):
    r = [0] * len(p)
    for i, j in enumerate(p): r[j] = i
    return tuple(r)

def closure(gens):
    e = tuple(range(len(gens[0])))
    seen = {e}; frontier = [e]
    while frontier:
        nf = []
        for a in frontier:
            for g in gens:
                b = mul(a, g)
                if b not in seen: seen.add(b); nf.append(b)
        frontier = nf
    return sorted(seen)

def cyc(n, *cycles):
    p = list(range(n))
    for c in cycles:
        for i in range(len(c)): p[c[i]] = c[(i + 1) % len(c)]
    return tuple(p)

def psl2(p):
    pts = list(range(p)) + ['inf']
    idx = {v: i for i, v in enumerate(pts)}
    def t(v): return 'inf' if v == 'inf' else (v + 1) % p
    def s(v):
        if v == 'inf': return 0
        if v == 0: return 'inf'
        return (-pow(v, p - 2, p)) % p
    return [tuple(idx[t(v)] for v in pts), tuple(idx[s(v)] for v in pts)]

GROUPS = {
    'S3': [cyc(3, (0, 1)), cyc(3, (0, 1, 2))],
    'A4': [cyc(4, (0, 1, 2)), cyc(4, (1, 2, 3))],
    'S4': [cyc(4, (0, 1)), cyc(4, (0, 1, 2, 3))],
    'D5': [cyc(5, (0, 1, 2, 3, 4)), cyc(5, (1, 4), (2, 3))],
    'F20': [cyc(5, (0, 1, 2, 3, 4)), cyc(5, (1, 2, 4, 3))],
    'A5': [cyc(5, (0, 1, 2, 3, 4)), cyc(5, (0, 1, 2))],
    'S5': [cyc(5, (0, 1)), cyc(5, (0, 1, 2, 3, 4))],
    'PSL(2,7)': psl2(7),
    'A6': [cyc(6, (0, 1, 2)), cyc(6, (1, 2, 3, 4, 5))],
    'PSL(2,11)': psl2(11),
}

def word_eval(w, g, h, e):
    ginv, hinv = inv(g), inv(h)
    r = e
    for a in w:
        r = mul(r, {1: g, -1: ginv, 2: h, -2: hinv}[a])
    return r

def ac_orbits(G):
    """Union-find over G x G under AC_G moves (conjugation by a generating set of G suffices)."""
    idx = {g: i for i, g in enumerate(G)}; n = len(G)
    parent = list(range(n * n))
    def find(a):
        while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
        return a
    def union(a, b):
        a, b = find(a), find(b)
        if a != b: parent[a] = b
    invs = [idx[inv(g)] for g in G]
    mt = [[idx[mul(a, b)] for b in G] for a in G]
    gens = GEN_IDX
    for i in range(n):
        for j in range(n):
            s = i * n + j
            union(s, invs[i] * n + j); union(s, i * n + invs[j])
            union(s, mt[i][j] * n + j); union(s, mt[i][invs[j]] * n + j)
            union(s, i * n + mt[j][i]); union(s, i * n + mt[j][invs[i]])
            for c in gens:
                ci = invs[c]
                union(s, mt[mt[ci][i]][c] * n + j); union(s, i * n + mt[mt[ci][j]][c])
    return idx, find

def main():
    global GEN_IDX
    tests = [('AK3', ([1, 1, 1, -2, -2, -2, -2], [1, 2, 1, -2, -1, -2]))]
    tests += [('ms%04d' % r['seq'], (r['r0'], r['r1'])) for r in ms.load('open')]
    controls = [('ms%04d' % r['seq'], (r['r0'], r['r1'])) for r in ms.load('certified')]
    report = {}
    for gname, gens in GROUPS.items():
        G = closure(gens); e = tuple(range(len(G[0])))
        idx = {g: i for i, g in enumerate(G)}
        GEN_IDX = [idx[g] for g in gens]
        idx, find = ac_orbits(G)
        n = len(G)
        import random; random.seed(1)
        if n <= 60:
            pairs = [(g, h) for g in G for h in G if len(closure([g, h])) == n]
        else:               # larger group: 200 random generating pairs
            pairs = []
            while len(pairs) < 200:
                g, h = random.choice(G), random.choice(G)
                if len(closure([g, h])) == n: pairs.append((g, h))
        ncomp = len({find(i * n + j) for (g, h) in pairs for i, j in [(idx[g], idx[h])]})
        sep = {}
        for label, group in (('open', tests), ('control', controls)):
            bad = []
            for tid, (r0, r1) in group:
                for g, h in pairs:
                    a, b = word_eval(r0, g, h, e), word_eval(r1, g, h, e)
                    if find(idx[a] * n + idx[b]) != find(idx[g] * n + idx[h]):
                        bad.append(tid); break
            sep[label] = bad
        report[gname] = dict(order=n, generating_pairs_tested=len(pairs), components_among_generating_pairs=ncomp, pairs_exhaustive=(n <= 60),
                             separated_open=sep['open'], separated_controls=sep['control'])
        print(gname, n, len(pairs), 'components', ncomp, 'separated open:', len(sep['open']), 'controls:', len(sep['control']), flush=True)
    json.dump(report, open(os.path.join(ROOT, 'data', 'fq_report.json'), 'w'), indent=1)

if __name__ == '__main__':
    main()
