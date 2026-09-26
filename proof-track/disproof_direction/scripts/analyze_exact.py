"""Exact AC classes (no symmetry quotient) of the open MS instances, from data/classify_exact.json.

Union rid with every hit Y (plain id => explicit AC path rid -> Y, no relabelling).  Also records, for each
run, which nontrivial elements of Sigma are AC-realised on the start within the cap (hits 'rid@map').
Writes data/classes_exact.json.
"""
import collections, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ms
db = json.load(open(os.path.join(ROOT, 'data', 'classify_exact.json')))['runs']
rows = {('ms%04d' % r['seq']): r for r in ms.load()}
open_ids = [k for k, r in rows.items() if r['status'] == 'open']
parent = {}
def find(a):
    parent.setdefault(a, a)
    while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
    return a
realised = {}; alarms = []
for rid, run in db.items():
    if run['status'].startswith('TRIVIAL'): alarms.append(rid)
    find(rid)
    realised[rid] = sorted(h['id'].split('@')[1] for h in run['hits'] if h['id'].startswith(rid + '@'))
    for h in run['hits']:
        if '@' not in h['id']: parent[find(h['id'])] = find(rid)
cls = collections.defaultdict(list)
for k in open_ids: cls[find(k)].append(k)
q = json.load(open(os.path.join(ROOT, 'data', 'classes.json')))['classes']
qmap = {m: i for i, c in enumerate(q) for m in c['members']}
out = [dict(members=sorted(v), quotient_class=sorted({qmap[m] for m in v}), contains=sorted(k for k in parent if find(k) == r and k not in open_ids))
       for r, v in cls.items()]
out.sort(key=lambda c: (min(rows[m]['L'] for m in c['members']), -len(c['members'])))
full = sum(1 for r in realised.values() if len(r) == 15)
summ = dict(exact_classes=len(out), quotient_classes=len(q), runs=len(db), alarms=alarms,
            runs_with_all_15_nontrivial_sigma_realised=full,
            sigma_realised_histogram=dict(collections.Counter(len(v) for v in realised.values())))
json.dump(dict(summary=summ, classes=out, sigma_realised=realised), open(os.path.join(ROOT, 'data', 'classes_exact.json'), 'w'), indent=1)
print(json.dumps(summ, indent=1))
for c in out[:12]: print(len(c['members']), c['quotient_class'], c['contains'], c['members'][:6])
