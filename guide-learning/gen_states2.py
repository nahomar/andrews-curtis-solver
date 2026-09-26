"""Sample decision states for value learning: states on our best paths plus all their legal neighbours.

Output: states_N.txt (acsolver problem format, id = lab-<k>) + meta_N.jsonl (id -> source puzzle, step t, role).
On-path states get label (moves remaining on the path) directly; neighbours are labelled later by a short search.
"""
import json, random, sys
sys.path.insert(0, '/Users/nahom/acc/vbias')
from common import apply_move, neighbours
A = '/Users/nahom/acc/acsolver/'
N = int(sys.argv[1]) if len(sys.argv) > 1 else 60000
rec = json.load(open(A + 'records.json')); db = json.load(open(A + 'best_ac.json'))
LET = {1: 0, -1: 1, 2: 2, -2: 3}
P = {}
for l in open(A + 'problems_ac.txt'):
    i, rest = l.split(None, 1); a, b = rest.split('|')
    P[i] = tuple(bytes(LET[int(g)] for g in part.split()) for part in (a, b))
good = [i for i, v in db.items() if rec[i][0] and v['length'] <= 1.5 * rec[i][0] and v['length'] <= 160]
rng = random.Random(7); rng.shuffle(good)
def ints(w): return ' '.join(str((c >> 1) + 1) * 1 if not (c & 1) else str(-((c >> 1) + 1)) for c in w)
out, meta, k, onpath = [], [], 0, []
for i in good:
    mv = db[i]['moves']; L = len(mv); s = P[i]; states = [s]
    for m in mv: s = apply_move(s, m); states.append(s)
    for t in rng.sample(range(L), min(4, L)):
        st = states[t]
        onpath.append({'puzzle': i, 't': t, 'r0': list(st[0]), 'r1': list(st[1]), 'label': L - t})
        for m, u in neighbours(st, 62):
            k += 1; lid = f'lab-{k:07d}'
            out.append(f'{lid} {ints(u[0])} | {ints(u[1])}\n')
            meta.append({'id': lid, 'puzzle': i, 't': t, 'move': m, 'taken': m == mv[t], 'r0': list(u[0]), 'r1': list(u[1])})
    if k >= N: break
open(f'states_{N}.txt', 'w').writelines(out)
with open(f'meta_{N}.jsonl', 'w') as f:
    for r in meta: f.write(json.dumps(r) + '\n')
with open(f'onpath_{N}.jsonl', 'w') as f:
    for r in onpath: f.write(json.dumps(r) + '\n')
print(len(good), 'good paths;', len(out), 'neighbour states;', len(onpath), 'on-path states')
