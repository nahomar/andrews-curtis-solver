"""Per-instance summary: data/per_instance.csv (one row per open MS instance)."""
import csv, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE); import ms
q = json.load(open(os.path.join(ROOT, 'data', 'classes.json')))
e = json.load(open(os.path.join(ROOT, 'data', 'classes_exact.json')))
runs = json.load(open(os.path.join(ROOT, 'data', 'classify.json')))['runs']
qc = {m: i for i, c in enumerate(q['classes']) for m in c['members']}
ec = {m: i for i, c in enumerate(e['classes']) for m in c['members']}
best = {}
for rid, r in runs.items():
    for h in r['hits']:
        if h['id'] not in best or r['cap'] > best[h['id']][0]: best[h['id']] = (r['cap'], r['size'], rid)
import glob, re
for fn in glob.glob(os.path.join(ROOT, 'data', 'fcc_*_cap*.log')):   # frontier runs (no paths, membership only)
    txt = open(fn).read()
    m = re.search(r'RESULT cap=(\d+) mode=quot16 size=(\d+) radius=\d+ NOT-TRIVIAL', txt)
    if not m: continue
    cap, size = int(m.group(1)), int(m.group(2))
    for hid in re.findall(r'HIT (\S+) dist=', txt):
        if hid.startswith('ms') and (hid not in best or cap > best[hid][0]):
            best[hid] = (cap, size, 'fcc:' + os.path.basename(fn))
with open(os.path.join(ROOT, 'data', 'per_instance.csv'), 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'n', 'w', 'total_length', 'two_hump_label', 'sigma_class', 'exact_class', 'certified_cap', 'quotient_component_size', 'run_start'])
    for r in sorted(ms.load('open'), key=lambda r: (r['L'], r['seq'])):
        k = 'ms%04d' % r['seq']; cap, size, rid = best[k]
        w.writerow([k, r['n'], r['w'], r['L'], r['cls'], qc[k], ec[k], cap, size, rid])
print('ok')
