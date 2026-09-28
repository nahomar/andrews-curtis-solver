"""Merge solver outputs into a best-path DB, write a submission TXT, and verify it with the OFFICIAL verifier.

    python3 check.py results/*.jsonl [--db best.json] [--manifest train] [--txt out.txt]
The DB keeps the shortest officially verified path per challenge id. Nothing is submitted from here.
"""
import argparse, json, os, subprocess, sys, tempfile
OFF = os.path.expanduser('~/acc/official/competition')
ap = argparse.ArgumentParser()
ap.add_argument('files', nargs='+'); ap.add_argument('--db', default='best.json')
ap.add_argument('--manifest', default='official', choices=['official', 'train'])
ap.add_argument('--txt', default='')
a = ap.parse_args()
man = f'{OFF}/tools/verifier/data/manifest.json' if a.manifest == 'official' else f'{OFF}/examples/training_manifest.json'

import fcntl
_lock = open(a.db + '.lock', 'w'); fcntl.flock(_lock, fcntl.LOCK_EX)   # one writer per DB at a time
db = json.load(open(a.db)) if os.path.exists(a.db) else {}
cand = {}
for f in a.files:
    for line in open(f):
        try: r = json.loads(line)
        except ValueError: continue               # skip a line garbled by an interrupted write
        want = 'sac-' if 'sac' in os.path.basename(a.db) else 'ac-'
        if not r['id'].startswith(want): continue            # each DB holds one problem type only
        if r.get('solved') and (r['id'] not in db or len(r['moves']) < len(db[r['id']]['moves'])):
            if r['id'] not in cand or len(r['moves']) < len(cand[r['id']]['moves']):
                cand[r['id']] = {'moves': r['moves'], 'src': os.path.basename(f)}
if not cand:
    print('no improvements'); sys.exit(0)
items = list(cand.items()); ok, bad = set(), []
for k in range(0, len(items), 500):                     # the verifier (like the server) takes <= 500 lines per file
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as t:
        for i, v in items[k:k + 500]: t.write(f"{i}: {json.dumps(v['moves'])}\n")
    res = subprocess.run([sys.executable, '-m', 'verifier', '--manifest', man, '--submission', t.name],
                         env={**os.environ, 'PYTHONPATH': f'{OFF}/tools'}, capture_output=True, text=True)
    out = json.loads(res.stdout)
    if not out.get('results'): print('verifier error:', res.stdout[:300], res.stderr[:300])
    ok |= {r['challenge_id'] for r in out.get('results', []) if r.get('ok')}
    bad += [r for r in out.get('results', []) if not r.get('ok')]
    os.unlink(t.name)
for i in ok: db[i] = {**cand[i], 'length': len(cand[i]['moves'])}
json.dump(db, open(a.db + '.tmp', 'w')); os.replace(a.db + '.tmp', a.db)
print(f'official verifier: {len(ok)} ok, {len(bad)} rejected; db now {len(db)} paths')
for r in bad[:5]: print('  rejected', r)
if a.txt:
    with open(a.txt, 'w') as f:
        for i in sorted(db): f.write(f"{i}: {json.dumps(db[i]['moves'])}\n")
    print('wrote', a.txt)
