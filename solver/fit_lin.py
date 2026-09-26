"""Fit a linear distance-to-go heuristic on states along verified best paths (label = moves remaining).
Features mirror what acs can compute cheaply; held-out by instance. Prints weights and rank-correlation gains."""
import json, random, numpy as np, sys
sys.path.insert(0, '/Users/nahom/acc/vbias')
from common import apply_move
rec = json.load(open('records.json')); db = json.load(open('best_ac.json'))
P = {}
for l in open('problems_ac.txt'):
    i, rest = l.split(None, 1); a, b = rest.split('|')
    P[i] = tuple(bytes((abs(int(g)) - 1) * 2 + (int(g) < 0) for g in part.split()) for part in (a, b))
def cyc(w):
    i, j = 0, len(w) - 1
    while i < j and w[i] ^ 1 == w[j]: i += 1; j -= 1
    return max(0, j - i + 1)
def inv(w): return bytes(c ^ 1 for c in reversed(w))
def best_child(r0, r1):
    best = cyc(r0) + cyc(r1)
    for wi, wj in ((r0, r1), (r1, r0)):
        ci, cj, ni, nj = cyc(wi), cyc(wj), len(wi), len(wj)
        for b in (wj, inv(wj)):
            for pa in range(ni):
                for pb in range(nj):
                    k = 0
                    while k < ni and k < nj and wi[(pa - 1 - k) % ni] ^ 1 == b[(pb + k) % nj]: k += 1
                    if k: best = min(best, ci + cj - 2 * k + cj)
    return best
def feats(s):
    r0, r1 = s; c0, c1 = cyc(r0), cyc(r1); t = len(r0) + len(r1)
    return [c0 + c1, t - c0 - c1, abs(c0 - c1), min(c0, c1), best_child(r0, r1), 1.0]
rng = random.Random(0); ids = [i for i in db if rec[i][0] and len(db[i]['moves']) - rec[i][0] <= 15]
rng.shuffle(ids); val = set(ids[: len(ids) // 5])
X, Y, V = [], [], []
for i in ids:
    mv = db[i]['moves']; s = P[i]; L = len(mv)
    for t in range(L + 1):
        if rng.random() < 0.15 and len(s[0]) + len(s[1]) <= 60:
            X.append(feats(s)); Y.append(L - t); V.append(i in val)
        if t < L: s = apply_move(s, mv[t])
X, Y, V = np.array(X, float), np.array(Y, float), np.array(V)
w, *_ = np.linalg.lstsq(X[~V], Y[~V], rcond=None)
def spearman(a, b):
    ra, rb = np.argsort(np.argsort(a)), np.argsort(np.argsort(b)); return np.corrcoef(ra, rb)[0, 1]
p = X[V] @ w
print('states', len(Y), 'val', int(V.sum()))
print('weights (cyc, junk, |c0-c1|, min(c0,c1), best_child, const):', np.round(w, 3).tolist())
print(f'val spearman: linear {spearman(p, Y[V]):.3f} | cyc only {spearman(X[V,0], Y[V]):.3f} | cyc+junk/2 {spearman(X[V,0] + X[V,1]/2, Y[V]):.3f} | min(cyc,best_child) {spearman(np.minimum(X[V,0], X[V,4]), Y[V]):.3f}')
json.dump({'w': w.tolist()}, open('lin_weights.json', 'w'))
