"""Universal identities over free formal relator variables; all primitives charged."""
import functools
import time
from .official import MOVE_TABLE, free_reduce
from .graph import states_of, within

TABLES = {}

ROWS = {n: [r for r in MOVE_TABLE if r['category'] not in ['stabilize', 'destabilize'] and r['relator'] < n and (r.get('other', 0) < n) and (abs(r.get('conjugator', 1)) <= n)] for n in [1, 2, 3]}

BYID = {r['id']: r for r in MOVE_TABLE}

def inv(w):
    return tuple((-x for x in reversed(w)))

def formal_step(s, m):
    r = BYID[m]
    i = r['relator']
    w = s[i]
    if r['category'] == 'inversion':
        w = inv(w)
    elif r['category'] == 'multiplication':
        w = tuple(free_reduce(w + (inv(s[r['other']]) if r['invert_other'] else s[r['other']])))
    elif r['category'] == 'conjugation':
        w = tuple(free_reduce((r['conjugator'],) + w + (-r['conjugator'],)))
    else:
        raise ValueError('Rank-changing operation is not a universal rewrite edge')
    return s[:i] + (w,) + s[i + 1:]

@functools.lru_cache(maxsize=100000)
def signature(rank, moves):
    s = tuple(((100 + i,) for i in range(rank)))
    for m in moves:
        s = formal_step(s, m)
    return s

def prepare():
    for rank in [1, 2, 3]:
        if rank in TABLES:
            continue
        root = signature(rank, ())
        table = {root: ()}
        frontier = {root: ()}
        for depth in range(3):
            nxt = {}
            for (s, path) in frontier.items():
                for row in ROWS[rank]:
                    m = row['id']
                    t = formal_step(s, m)
                    candidate = path + (m,)
                    if t not in table and (t not in nxt or candidate < nxt[t]):
                        nxt[t] = candidate
            table.update(nxt)
            frontier = nxt
        TABLES[rank] = table
    return {n: len(t) for (n, t) in TABLES.items()}

def rewrite(start, goal, old, budget, cap, deadline, seconds=1):
    begin = time.monotonic()
    prepare()
    path = tuple(old)
    trace = states_of(start, path)
    assert trace[-1] == goal
    checked = changes = saved = 0
    limited = None
    i = 0
    while i < len(path):
        if checked >= cap or time.time() >= deadline or time.monotonic() - begin >= seconds:
            limited = 'expansions' if checked >= cap else 'time'
            break
        rank = len(trace[i])
        changed = False
        if rank in TABLES:
            for width in [4, 3, 2, 1]:
                if checked >= cap:
                    limited = 'expansions'
                    break
                block = path[i:i + width]
                if len(block) != width or any((BYID[m]['category'] in ['stabilize', 'destabilize'] for m in block)):
                    continue
                checked += 1
                replacement = TABLES[rank].get(signature(rank, block))
                if replacement is None or (len(replacement), replacement) >= (len(block), block):
                    continue
                local = states_of(trace[i], replacement)
                assert local[-1] == trace[i + width]
                if not all((within(s) for s in local)):
                    continue
                path = path[:i] + replacement + path[i + width:]
                trace = states_of(start, path)
                assert trace[-1] == goal
                changes += 1
                saved += width - len(replacement)
                i = max(0, i - 3)
                changed = True
                break
        if not changed:
            i += 1
    return dict(moves=path if len(path) <= budget else None, best_local_moves=path, atomic_saving=len(old) - len(path), expanded=checked, rewrites=changes, seconds=time.monotonic() - begin, limited=limited, verified_edges=[] if len(path) >= len(old) else [dict(from_index=0, to_index=len(old), moves=path)])
