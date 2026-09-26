"""Compare solver results with current public records (records.json: id -> [ac_best, ac_k, sac_best, sac_k])."""
import json, sys, numpy as np
rec = json.load(open('records.json'))
best = {}
for f in sys.argv[1:]:
    for l in open(f):
        r = json.loads(l)
        if r['solved'] and (r['id'] not in best or r['length'] < best[r['id']]): best[r['id']] = r['length']
ids = sorted({json.loads(l)['id'] for f in sys.argv[1:] for l in open(f)})
col = 0 if ids[0].startswith('ac-') else 2
rows = []
for i in ids:
    key = i if col == 0 else 'ac-' + i.split('-')[1]
    R, k = rec[key][col], rec[key][col + 1]
    rows.append((i, R, k, best.get(i)))
for lo, hi in ((0, 40), (40, 60), (60, 100), (100, 300), (300, 10**6)):
    g = [r for r in rows if r[1] and lo <= r[1] < hi]
    if not g: continue
    s = [r for r in g if r[3] is not None]; gap = np.array([r[3] - r[1] for r in s])
    print(f'record {lo:>3}-{hi:<7} n={len(g):3d} solved {len(s):3d}  beat {int((gap<0).sum()):3d}  tie {int((gap==0).sum()):3d}  median gap {np.median(gap) if len(gap) else float("nan"):+.1f}')
