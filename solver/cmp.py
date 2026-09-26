"""Per-instance comparison of result files vs record and Catsitter's published length."""
import json, sys
rec = json.load(open('records.json')); cat = json.load(open('../acc-challenge/solver/best_ac.json'))
R = [{json.loads(l)['id']: json.loads(l) for l in open(f)} for f in sys.argv[1:]]
ids = sorted(set().union(*R))
print('id'.ljust(10), 'rec', 'cat', *[f.split('/')[-1][:14].rjust(15) for f in sys.argv[1:]])
tot = [0] * len(R)
for i in ids:
    row = [r.get(i, {}).get('length', -1) for r in R]
    print(i.ljust(10), str(rec[i][0]).rjust(3), str(len(cat[i]['moves']) if i in cat else '-').rjust(3), *[str(x).rjust(15) for x in row])
for k, f in enumerate(sys.argv[1:]):
    both = [i for i in ids if all(r.get(i, {}).get('length', -1) > 0 for r in R)]
    print(f, 'sum over commonly solved', sum(R[k][i]['length'] for i in both), 'n', len(both))
