"""Stage 1: length-capped component classification of the open Miller-Schupp instances.

For each open instance X (sorted by total length, then seq) that is not yet in an already-computed
component, compute its Sigma-quotient component in G_C with lcc, trying C = CAPS[0], CAPS[1], ... until
the component fits in memory.  Every watch-list state (all 1190 MS instances, AK(3), P1) found in the
component is recorded with an explicit move path X -> sigma(Y).  Results -> data/classify.json,
paths -> data/paths/<id>_cap<C>.txt.

Usage: python3 scripts/classify.py [--caps 25,24,23,22] [--log2 28] [--only ms0059,...] [--exact]

--exact: no Sigma quotient (exact AC components); the watch list then contains all 16 Sigma-images of every
instance (id@map), so a hit 'Y@map' is an explicit AC path X -> map(Y); caps are chosen from the quotient
run (same cap if 16 x quotient size < 170M, else one less, decreasing on abort); output classify_exact.json,
paths in data/paths_exact/.
"""
import json, os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import ms

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(ROOT, 'data')
LCC = os.path.join(HERE, 'lcc')

EXACT = '--exact' in sys.argv

def run(r0, r1, cap, log2, paths_out):
    mode = [] if EXACT else ['--quot']
    watch = os.path.join(DATA, 'watch_sigma.txt' if EXACT else 'watch.txt')
    cmd = [LCC] + mode + ['--cap', str(cap), '--log2', str(log2), '--threads', '2',
           '--watch', watch, '--paths', paths_out, '--'] + \
          [str(a) for a in r0] + ['|'] + [str(a) for a in r1]
    t = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True)
    out = p.stdout
    res = {'cap': cap, 'exit': p.returncode, 'secs': round(time.time() - t, 1)}
    m = re.search(r'RESULT cap=\d+ mode=\S+ size=(\d+) radius=(\d+) (\S+)', out)
    if not m:
        res['status'] = 'ABORT'
        return res
    res.update(size=int(m.group(1)), radius=int(m.group(2)), status=m.group(3))
    res['hits'] = [dict(id=h[0], dist=int(h[1]), sigma=int(h[2]))
                   for h in re.findall(r'HIT (\S+) dist=(\d+) sigma=(\d+)', out)]
    res['layers'] = [int(x) for x in re.findall(r'layer \d+ new=\d+ total=(\d+)', out)]
    return res

# table size per cap: large caps get a small table so hopeless attempts abort quickly
LOG2 = {25: 26, 24: 27, 23: 28} if not EXACT else {}

def main():
    caps = [25, 24, 23, 22]
    log2 = 28; only = None
    a = sys.argv[1:]
    for i, t in enumerate(a):
        if t == '--caps': caps = [int(x) for x in a[i + 1].split(',')]
        if t == '--log2': log2 = int(a[i + 1])
        if t == '--only': only = a[i + 1].split(',')
    pdir = os.path.join(DATA, 'paths_exact' if EXACT else 'paths')
    os.makedirs(pdir, exist_ok=True)
    out_f = os.path.join(DATA, 'classify_exact.json' if EXACT else 'classify.json')
    if EXACT:   # choose caps from the quotient results: exact components are up to 16x larger
        qdb = json.load(open(os.path.join(DATA, 'classify.json')))['runs']
        qcap = {}
        for rid, run_ in qdb.items():
            for h in run_['hits']: qcap[h['id']] = (run_['cap'], run_['size'])
    db = json.load(open(out_f)) if os.path.exists(out_f) else {'runs': {}}
    rows = sorted(ms.load('open'), key=lambda r: (r['L'], r['seq']))
    covered = set()
    for rid, run_ in db['runs'].items():
        for h in run_.get('hits', []):
            if '@' not in h['id']: covered.add(h['id'])
    for r in rows:
        rid = 'ms%04d' % r['seq']
        if only and rid not in only: continue
        if not only and (rid in covered or rid in db['runs']): continue
        mycaps = caps
        if EXACT:
            c0, sz = qcap[rid]
            c0 = c0 if 16 * sz < 170e6 else c0 - 1
            mycaps = [c for c in range(c0, 18, -1)]
        for cap in mycaps:
            if cap < r['L']: continue
            pf = os.path.join(pdir, f'{rid}_cap{cap}.txt')
            res = run(r['r0'], r['r1'], cap, LOG2.get(cap, log2), pf)
            print(rid, 'L=%d' % r['L'], 'cap', cap, res['status'], res.get('size'), res['secs'], 's',
                  'hits', len(res.get('hits', [])), flush=True)
            if res['status'] != 'ABORT':
                res['L'] = r['L']
                db['runs'][rid] = res
                for h in res['hits']:
                    if '@' not in h['id']: covered.add(h['id'])
                json.dump(db, open(out_f, 'w'), indent=0)
                break
            if os.path.exists(pf): os.remove(pf)

if __name__ == '__main__':
    main()
