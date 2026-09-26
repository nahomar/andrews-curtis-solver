"""Transfer paths between related puzzles (same relators up to rotation/inversion/mirror/order).

For puzzles A, B in one class: find a short prefix of cheap moves (inversions 0/1, conjugations 6-13) taking A's start
exactly to s(B's start) for a mirror s (signed letter permutation, optional relator swap). Then append B's best path
with every move relabelled through s, then an endpoint fix: AC needs s(x, y) -> (x, y) (precomputed by BFS);
Stable AC needs only the official finish (invert negative relators, then 16, 15). Every candidate is replayed here and
later re-verified by the official verifier in check.py.
"""
import json, sys, collections
sys.path.insert(0, '/Users/nahom/acc/vbias')
from common import apply_move, TARGET
LET = {1: 0, -1: 1, 2: 2, -2: 3}
def word(ints): return bytes(LET[g] for g in ints)
P = {}
for l in open('problems_ac.txt'):
    i, rest = l.split(None, 1); a, b = rest.split('|')
    P[i] = (word(map(int, a.split())), word(map(int, b.split())))
db = json.load(open('best_ac.json')); rec = json.load(open('records.json'))
classes = json.load(open('results/rel_classes.json'))

# mirrors: letter maps (x,X,y,Y -> ...) from signed permutations, plus relator swap
def letter_maps():
    out = []
    for swap in (0, 1):
        for sx in (0, 1):
            for sy in (0, 1):
                gx, gy = (2, 0) if swap else (0, 2)
                m = [gx ^ sx, gx ^ sx ^ 1, gy ^ sy, gy ^ sy ^ 1]
                out.append(m)
    return out
MAPS = [(m, rs) for m in letter_maps() for rs in (0, 1)]
def map_state(s, m, rs):
    a, b = bytes(m[c] for c in s[0]), bytes(m[c] for c in s[1])
    return (b, a) if rs else (a, b)
def map_move(mv, m, rs):
    if mv in (0, 1): r = mv
    elif 2 <= mv <= 5: r = mv
    else:
        base, c = (6, mv - 6) if mv < 10 else (10, mv - 10)
        r = base + m[c]
    if rs:
        r = {0: 1, 1: 0, 2: 4, 3: 5, 4: 2, 5: 3}.get(r, r if r < 6 else (r + 4 if r < 10 else r - 4))
    return r
CHEAP = [0, 1, 6, 7, 8, 9, 10, 11, 12, 13]
def bfs(start, goal, moves, depth, cap=40):
    if start == goal: return []
    prev = {start: None}; frontier = [start]
    for _ in range(depth):
        nxt = []
        for s in frontier:
            for mv in moves:
                t = apply_move(s, mv)
                if not t[0] or not t[1] or len(t[0]) + len(t[1]) > cap or t in prev: continue
                prev[t] = (s, mv)
                if t == goal:
                    path = []
                    while prev[t]: t, mv2 = prev[t]; path.append(mv2)
                    return path[::-1]
                nxt.append(t)
        frontier = nxt
    return None
FIX = {}      # AC endpoint fix: s(x,y) -> (x,y)
for m, rs in MAPS:
    e = map_state(TARGET, m, rs)
    FIX[(tuple(m), rs)] = bfs(e, TARGET, list(range(14)), 12, cap=8)
def sac_finish(e):
    fin = []
    if e[0] in (bytes([1]), bytes([3])): fin.append(0)
    if e[1] in (bytes([1]), bytes([3])): fin.append(1)
    return fin + [16, 15]

def word_bfs(w, goal, rel):
    """Exact cheapest invert/rotate sequence on ONE cyclically reduced relator taking w to goal, or None.
    Rotating left by one = conjugate by the inverse of the first letter; right by one = conjugate by the last letter."""
    base = 6 if rel == 0 else 10
    def cycred(x): return len(x) < 2 or x[0] ^ 1 != x[-1]
    if not (cycred(w) and cycred(goal)) or len(w) != len(goal): return None
    best = None
    for inv_first in (0, 1):
        x = w if not inv_first else bytes(c ^ 1 for c in reversed(w))
        pre = [rel] if inv_first else []
        n = len(x)
        for k in range(n):
            if x[k:] + x[:k] == goal:          # left rotation by k
                seq = []; y = x
                for _ in range(k): c = y[0] ^ 1; seq.append(base + c); y = y[1:] + y[:1]
                cand = pre + seq
                if best is None or len(cand) < len(best): best = cand
                seq = []; y = x                  # right rotation by n-k
                for _ in range(n - k if k else 0): c = y[-1]; seq.append(base + c); y = y[-1:] + y[:-1]
                cand = pre + seq
                if len(cand) < len(best): best = cand
    return best
def prefix(start, goal):
    p0 = word_bfs(start[0], goal[0], 0)
    if p0 is None: return None
    p1 = word_bfs(start[1], goal[1], 1)
    if p1 is None: return None
    return p0 + p1
ac_out, sac_out, n_ac, n_sac = [], [], 0, 0
best_sac = json.load(open('best_sac.json'))
for cls in classes:
    for A in cls:
        for B in cls:
            if A == B or B not in db: continue
            pb = db[B]['moves']
            for m, rs in MAPS:
                goal = map_state(P[B], m, rs)
                pre = prefix(P[A], goal)
                if pre is None: continue
                body = [map_move(mv, m, rs) for mv in pb]
                s = P[A]
                for mv in pre + body: s = apply_move(s, mv)
                e = map_state(TARGET, m, rs)
                if s != e: continue                                  # replay sanity check
                fix = FIX[(tuple(m), rs)]
                if fix is not None:
                    cand = pre + body + fix
                    cur = db.get(A, {}).get('length', 10**9)
                    if len(cand) < cur:
                        ac_out.append({'id': A, 'solved': True, 'length': len(cand), 'moves': cand, 'from': B}); n_ac += 1
                sc = pre + body + sac_finish(e); sid = 'sac-' + A.split('-')[1]
                if len(sc) < best_sac.get(sid, {}).get('length', 10**9):
                    sac_out.append({'id': sid, 'solved': True, 'length': len(sc), 'moves': sc, 'from': B}); n_sac += 1
with open('results/transfer1_ac.jsonl', 'w') as f:
    for r in ac_out: f.write(json.dumps(r) + '\n')
with open('results/transfer1_sac.jsonl', 'w') as f:
    for r in sac_out: f.write(json.dumps(r) + '\n')
def summarize(rows, col):
    beat = tie = 0
    for r in rows:
        R = rec['ac-' + r['id'].split('-')[1]][col]
        if R and r['length'] < R: beat += 1
        elif R and r['length'] == R: tie += 1
    return beat, tie
print('AC candidates shorter than our best:', n_ac, '| vs record (beat, tie):', summarize(ac_out, 0))
print('SAC candidates shorter than our best:', n_sac, '| vs record (beat, tie):', summarize(sac_out, 2))
