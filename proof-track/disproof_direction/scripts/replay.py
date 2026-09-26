"""Independent replay of equivalence certificates with the OFFICIAL verifier's move semantics.

Each path line (written by lcc --paths) reads
    <target-id> sigma=<g> end=<state> moves=m1,m2,...
and is relative to a start instance (given by the file name <start-id>_cap<C>.txt).
We replay the moves from the start with verifier.core.apply_move (the official ac-r2-v1 semantics),
record the peak total length, and check that the final state equals phi(target) for SOME element phi of
Sigma = <signed permutations of {x,y}, relator swap>, re-deriving phi independently of lcc's encoding.
Since every phi in Sigma maps AC-trivial presentations to AC-trivial ones (phi permutes the move set and
fixes the AC class of (x,y)), such a path shows: start is AC-trivial  <=>  target is AC-trivial.

Usage: python3 scripts/replay.py [data/paths/*.txt]   (default: all files)  -> data/replay_report.json
"""
import glob, json, os, sys
sys.path.insert(0, os.path.expanduser('~/acc/official/competition/tools'))
from verifier.core import apply_move, free_reduce  # official semantics
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ms

def watch_states():
    W = {}
    for line in open(os.path.join(ROOT, 'data', 'watch.txt')):
        line = line.split('#')[0].strip()
        if not line: continue
        ident, rest = line.split(None, 1)
        a, b = rest.split('|')
        W[ident] = (tuple(int(t) for t in a.split()), tuple(int(t) for t in b.split()))
    W['TRIV'] = ((1,), (2,))
    return W

def sigma_images(st):
    """All 16 images of a state under Sigma, as {description: state}."""
    out = {}
    for swap_gens in (False, True):
        for sx in (1, -1):
            for sy in (1, -1):
                def f(a):
                    g = abs(a); s = 1 if a > 0 else -1
                    img = (2 if swap_gens else 1) * sx if g == 1 else (1 if swap_gens else 2) * sy
                    return img * s
                r0 = tuple(f(a) for a in st[0]); r1 = tuple(f(a) for a in st[1])
                name = f"x->{'y' if swap_gens else 'x'}^{sx},y->{'x' if swap_gens else 'y'}^{sy}"
                out[name] = (r0, r1)
                out[name + ',swap'] = (r1, r0)
    return out

def replay(start, moves):
    s = (tuple(start[0]), tuple(start[1])); peak = len(s[0]) + len(s[1])
    for m in moves:
        s = apply_move(s, m)
        if len(s[0]) == 0 or len(s[1]) == 0: return None, peak
        peak = max(peak, len(s[0]) + len(s[1]))
    return s, peak

def main():
    W = watch_states()
    files = sys.argv[1:] or sorted(glob.glob(os.path.join(ROOT, 'data', 'paths', '*.txt')))
    report = []; bad = 0
    for f in files:
        start_id = os.path.basename(f).split('_cap')[0]
        cap = int(os.path.basename(f).split('_cap')[1].split('.')[0])
        start = W[start_id]
        for line in open(f):
            parts = line.split()
            if not parts: continue
            tid = parts[0]
            mv = [int(x) for x in parts[-1].split('=', 1)[1].split(',') if x != '']
            end, peak = replay(start, mv)
            phi = [k for k, v in sigma_images(W[tid]).items() if v == end] if end else []
            ok = bool(phi) and peak <= cap
            bad += not ok
            report.append(dict(start=start_id, target=tid, cap=cap, moves=len(mv), peak=peak, phi=phi[:1], ok=ok))
    json.dump(report, open(os.path.join(ROOT, 'data', 'replay_report.json'), 'w'), indent=0)
    print(f'replayed {len(report)} certificates, failures: {bad}')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
