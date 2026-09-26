"""Mine frequent move n-grams from verified paths as candidate macros.

Excludes n-grams already covered by the solver's built-in macros (conjugations then one multiply, or pure
conjugations of one relator) and any n-gram containing a move immediately followed by its inverse.
Score = count * (n - 1): how many beam levels the macro would save across the corpus.
    python3 mine_macros.py [--k 64] [--nmax 8] [--out macros.txt]
"""
import argparse, collections, json, glob
INV = [0, 1, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12]
ap = argparse.ArgumentParser(); ap.add_argument('--k', type=int, default=64); ap.add_argument('--nmax', type=int, default=8)
ap.add_argument('--out', default='macros.txt'); ap.add_argument('--nmin', type=int, default=3); ap.add_argument('--maxgap', type=int, default=2)
ap.add_argument('--minlift', type=float, default=3.0); a = ap.parse_args()
rec = json.load(open('records.json'))
paths = [v['moves'] for i, v in json.load(open('best_ac.json')).items() if rec[i][0] and len(v['moves']) - rec[i][0] <= a.maxgap]   # near-record paths only
uni = collections.Counter(m for p in paths for m in p); tot = sum(uni.values())
def conj(m): return m >= 6
def covered(g):
    if all(conj(m) for m in g): return True                                   # rotations
    if all(conj(m) for m in g[:-1]) and g[-1] in (2, 3, 4, 5): return True    # rotate + multiply
    return False
cnt = collections.Counter()
for p in paths:
    for n in range(a.nmin, a.nmax + 1):
        for i in range(len(p) - n + 1):
            g = tuple(p[i:i + n])
            if any(g[j + 1] == INV[g[j]] for j in range(n - 1)) or covered(g): continue
            cnt[g] += 1
def lift(g, c):
    exp = sum(len(p) - len(g) + 1 for p in paths if len(p) >= len(g))
    for m in g: exp *= uni[m] / tot
    return c / max(exp, 1e-9)
ranked = sorted(((g, c) for g, c in cnt.items() if c >= 20 and lift(g, c) >= a.minlift), key=lambda kv: -kv[1] * (len(kv[0]) - 1))
sel = []
for g, c in ranked:
    if len(sel) >= a.k: break
    if any(len(s) > len(g) and any(s[i:i + len(g)] == g for i in range(len(s) - len(g) + 1)) and cnt[s] >= 0.8 * c for s, _ in sel): continue  # subsumed
    sel.append((g, c))
with open(a.out, 'w') as f:
    for g, c in sel: f.write(' '.join(map(str, g)) + f'   # count {c}\n')
print(f'{len(paths)} paths, {sum(len(p) for p in paths)} moves; top macros:')
for g, c in sel[:15]: print(f'  {g}  count {c}  lift {lift(g, c):.1f}')
