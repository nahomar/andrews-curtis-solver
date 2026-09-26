"""End-to-end test: beam search from held-out instance starts, scored by a value model or by length.

For each held-out instance whose verified path has <= --max-len moves, run a beam of width --width
(no revisits, total length <= 64) until it reaches (x, y) or runs --depth-mult x the verified path length.
Reports the solve rate and the mean gap to the verified path length on instances both scorers solved.
    .venv/bin/python beam.py --model runs/base/model.pt runs/all/model.pt --width 256
"""
import argparse, json, time, numpy as np, torch
from common import TARGET, SEQ, neighbours, encode, total
from data import Paths, decode
from evaluate import score, DEV
from model import load


def beam(start, scorer, width, max_depth):
    beam_, seen = [(start, [])], {start}
    for depth in range(1, max_depth + 1):
        cand = []
        for s, path in beam_:
            for m, t in neighbours(s):
                if t in seen: continue
                if t == TARGET: return path + [m]
                seen.add(t); cand.append((t, path + [m]))
        if not cand: return None
        sc = scorer([t for t, _ in cand])
        keep = np.argsort(sc, kind='stable')[:width]
        beam_ = [cand[k] for k in keep]
    return None


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--model', nargs='*', default=[]); ap.add_argument('--width', type=int, default=256)
    ap.add_argument('--max-len', type=int, default=60); ap.add_argument('--n', type=int, default=100)
    ap.add_argument('--depth-mult', type=float, default=2.0); ap.add_argument('--out', default='')
    a = ap.parse_args()
    P = Paths('data/paths.npz')
    starts = {}                                     # held-out instance -> (start state, verified length)
    for i in np.where(P.VAL)[0]:
        g = P.G[i]
        if g not in starts or P.Y[i] > starts[g][1]: starts[g] = (decode(P.X[i]), float(P.Y[i]))
    inst = sorted(g for g, (_, L) in starts.items() if L <= a.max_len)[:a.n]
    scorers = {'length': lambda st: np.array([total(s) for s in st], np.float32)}
    for p in a.model:
        m = load(p, DEV)
        scorers[p] = (lambda m: lambda st: score(m, np.frombuffer(b''.join(encode(s) for s in st), np.uint8).reshape(-1, SEQ)))(m)
    res = {}
    for name, f in scorers.items():
        t0 = time.time(); res[name] = {}
        for g in inst:
            s, L = starts[g]
            p = beam(s, f, a.width, int(L * a.depth_mult))
            res[name][int(g)] = None if p is None else len(p) - L
        solved = [v for v in res[name].values() if v is not None]
        print(f'{name}: solved {len(solved)}/{len(inst)}, mean gap to verified {np.mean(solved) if solved else float("nan"):+.2f}, {time.time() - t0:.0f}s', flush=True)
    both = [g for g in map(int, inst) if all(res[n][g] is not None for n in res)]
    print(f'on {len(both)} instances every scorer solved: ' + ', '.join(f'{n} {np.mean([res[n][g] for g in both]):+.2f}' for n in res))
    if a.out: json.dump(res, open(a.out, 'w'))
