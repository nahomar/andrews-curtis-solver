"""Compare solver paths on the 424 training instances with the organisers' known sequences."""
import json, sys, numpy as np
t = {i['training_id']: i['length'] for i in json.load(open('/Users/nahom/acc/official/competition/examples/training_424.json'))['instances']}
best = {}
for f in sys.argv[1:]:
    for l in open(f):
        r = json.loads(l)
        if r['solved'] and (r['id'] not in best or r['length'] < best[r['id']]): best[r['id']] = r['length']
d = np.array([best[i] - t[i] for i in best])
print(f'solved {len(best)}/424 | vs known: shorter {(d<0).sum()} equal {(d==0).sum()} longer {(d>0).sum()} | mean diff {d.mean():+.2f} | total {sum(best.values())} vs {sum(t[i] for i in best)}')
