"""States along acc-challenge's verified best AC paths (solver/best_ac.json).

Label = moves remaining on the path, an upper bound on the true distance. Each consecutive pair is a
transition: the path moved from s_t to s_t+1, so a consistent value function should drop by about 1
there, whether the step grows, keeps or shrinks the presentation. 10% of instances are held out.
Writes data/paths.npz: X (N, SEQ) tokens, Y remaining, G instance index, VAL flag, and transitions
(TA, TB, TM) = (index of s_t, index of s_t+1, move taken).
"""
import argparse, json, numpy as np
from common import parse_pool, replay, encode, SEQ

ap = argparse.ArgumentParser()
ap.add_argument('--sol', default='../acc-challenge/solver')
ap.add_argument('--val-frac', type=float, default=0.1)
ap.add_argument('--seed', type=int, default=0)
a = ap.parse_args()

pool = parse_pool(f'{a.sol}/pool_ac.txt')
best = json.load(open(f'{a.sol}/best_ac.json'))
ids = sorted(best)
rng = np.random.default_rng(a.seed)
val = set(rng.choice(ids, int(len(ids) * a.val_frac), replace=False).tolist())

rows, Y, G, VAL, TA, TB, TM = [], [], [], [], [], [], []
for g, i in enumerate(ids):
    moves = best[i]['moves']; L = len(moves)
    prev = None
    for t, s in enumerate(replay(pool[i], moves)):
        r = encode(s)
        cur = None
        if r is not None:
            cur = len(rows); rows.append(r); Y.append(L - t); G.append(g); VAL.append(i in val)
        if prev is not None and cur is not None:
            TA.append(prev); TB.append(cur); TM.append(moves[t - 1])
        prev = cur
X = np.frombuffer(b''.join(rows), np.uint8).reshape(-1, SEQ)
np.savez('data/paths.npz', X=X, Y=np.array(Y, np.float32), G=np.array(G, np.int32), VAL=np.array(VAL),
         TA=np.array(TA, np.int64), TB=np.array(TB, np.int64), TM=np.array(TM, np.int8), ids=np.array(ids))
print(f'{len(ids)} paths, {len(X)} states ({int(np.sum(VAL))} val), {len(TA)} transitions')
