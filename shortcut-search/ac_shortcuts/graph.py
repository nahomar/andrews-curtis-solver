"""Exact rank-zero through rank-three transitions; every edge costs one move."""
from .official import apply_move, MOVE_TABLE, INVERSE_MOVE

ROWS = {k: [r['id'] for r in MOVE_TABLE if r['category'] not in ('stabilize', 'destabilize') and r['relator'] < k and (r.get('other', 0) < k) and (abs(r.get('conjugator', 1)) <= k)] for k in range(4)}

def within(s):
    return len(s) <= 3 and sum(map(len, s)) <= 80 and all((len(w) <= 48 for w in s))

def states_of(start, moves):
    states = [start]
    for m in moves:
        (s, e) = apply_move(states[-1], m)

        if e is not None:
            raise ValueError(e)
        states.append(s)
    return states

def forward(s):
    k = len(s)
    for m in ROWS[k] + list(range(15, 15 + k)) + ([14] if k < 3 else []):
        (t, e) = apply_move(s, m)
        if e is None and t != s and within(t):
            yield (m, t)

def backward(s):
    k = len(s)
    for m in ROWS[k]:
        (t, e) = apply_move(s, m)
        assert e is None
        if t != s and within(t):
            yield (INVERSE_MOVE[m], t)
    if k and s[-1] == (k,) and (not any((abs(a) == k for w in s[:-1] for a in w))):
        yield (14, s[:-1])
    if k < 3:
        for g in range(1, k + 2):
            lifted = tuple((tuple(((1 if a > 0 else -1) * (abs(a) + (abs(a) >= g)) for a in w)) for w in s))
            for i in range(k + 1):
                t = lifted[:i] + ((g,),) + lifted[i:]
                if within(t):
                    yield (15 + i, t)
