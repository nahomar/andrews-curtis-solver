"""Exact ball around the target (x, y): BFS over every state with total relator length <= cap.

Distances are exact within the length-capped graph (an upper bound on the true AC distance, the same
convention as acc-challenge's ball16/ball18). cap 12: 1.6M states, ~12 s; cap 14: 14.4M states, ~2 min, ~4 GB RAM.
Writes data/ball{cap}.npz with X (N, cap+1) token rows (r0 SEP r1 PAD...) and D (N,) distances.
"""
import argparse, time, numpy as np
from common import TARGET, neighbours, encode

ap = argparse.ArgumentParser()
ap.add_argument('--cap', type=int, default=12)
a = ap.parse_args()

t0 = time.time()
dist, frontier, d = {TARGET: 0}, [TARGET], 0
while frontier:
    d += 1; nxt = []
    for s in frontier:
        for _, t in neighbours(s, a.cap):
            if t not in dist:
                dist[t] = d; nxt.append(t)
    frontier = nxt
    print(f'  d={d} new={len(nxt)}', flush=True)
w = a.cap + 1
X = np.frombuffer(b''.join(encode(s, w) for s in dist), np.uint8).reshape(-1, w)
D = np.fromiter(dist.values(), np.uint8, len(dist))
np.savez(f'data/ball{a.cap}.npz', X=X, D=D)
print(f'ball{a.cap}: {len(D)} states, max dist {D.max()}, {time.time() - t0:.0f}s')
