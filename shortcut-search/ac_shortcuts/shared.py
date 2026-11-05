"""Paid forward/backward searches seeded by supplied witness boundaries."""
import heapq
import itertools
import time
from .graph import forward, backward, states_of, within
from .words import product, aligned
from .joins import key, connector
from .official import INVERSE_MOVE

def search(mode, start, goal, old, budget, cap, deadline, seconds=1, extra_paths=()):
    assert mode == 'shared_conjugacy'
    begin = time.monotonic()
    serial = itertools.count()
    expanded = [0, 0]
    seen = [{}, {}]
    q = [[], []]
    limited = None
    join_attempts = join_hits = seed_join_hits = 0
    indices = [{}, {}]

    def result(moves=None):
        return dict(moves=moves, expanded=sum(expanded), expanded_sides=expanded, states=sum(map(len, seen)), limited=limited, seconds=time.monotonic() - begin, join_attempts=join_attempts, join_hits=join_hits, seed_join_hits=seed_join_hits)
    if time.time() > deadline:
        limited = 'time'
        return result()
    old = tuple(old)
    trajectory = states_of(start, old)
    assert trajectory[-1] == goal
    if start == goal:
        return result(())
    if cap <= 0:
        limited = 'expansions'
        return result()
    for candidate in (old,) + tuple((tuple(x) for x in extra_paths)):
        trajectory = states_of(start, candidate)
        assert trajectory[-1] == goal
        for (i, s) in enumerate(trajectory):
            if not within(s):
                continue
            for (side, path) in ((0, candidate[:i]), (1, candidate[i:])):
                cost = len(path)
                if cost <= budget and cost < seen[side].get(s, (100000, None))[0]:
                    seen[side][s] = (cost, path)
    for (s, (cost, path)) in seen[0].items():
        other = seen[1].get(s)
        if other and cost + other[0] <= budget:
            answer = path + other[1]
            assert states_of(start, answer)[-1] == goal
            return result(answer)

    def index(side, s):
        bucket = indices[side].setdefault(key(s), [])
        if s not in bucket:
            bucket.append(s)
        bucket.sort(key=lambda u: (seen[side][u][0], u))
        if len(bucket) > 8:
            del bucket[8:]
    for side in (0, 1):
        for s in seen[side]:
            index(side, s)

    def bridge(side, s, cost, path):
        nonlocal join_attempts, join_hits
        for t in indices[1 - side].get(key(s), []):
            (c, other) = seen[1 - side][t]
            remaining = budget - cost - c
            if remaining <= 0:
                continue
            join_attempts += 1
            moves = connector(s, t, remaining)
            if moves is None:
                continue
            join_hits += 1
            answer = path + moves + other if side == 0 else other + tuple((INVERSE_MOVE[m] for m in reversed(moves))) + path
            assert len(answer) <= budget and states_of(start, answer)[-1] == goal
            return answer
        return None
    for (s, (cost, path)) in seen[0].items():
        if time.time() > deadline or time.monotonic() - begin > seconds:
            limited = 'time'
            return result()
        answer = bridge(0, s, cost, path)
        if answer is not None:
            seed_join_hits += 1
            return result(answer)
    anchors = []
    for side in (0, 1):
        roots = sorted(((c, s) for (s, (c, p)) in seen[side].items()), key=lambda x: (x[0], x[1]))
        indexes = sorted({round(i * (len(roots) - 1) / 7) for i in range(8)}) if roots else []
        anchors.append([roots[i] for i in indexes])

    def priority(s, side, cost):
        if not anchors[1 - side]:
            return (cost, 0.0)
        (h, anchor) = min(((c + 1.5 * aligned(s, g), g) for (c, g) in anchors[1 - side]), key=lambda x: x[0])
        return (cost + h, product(s, anchor))
    for side in (0, 1):
        for (s, (cost, path)) in seen[side].items():
            if cost < budget:
                (score, tie) = priority(s, side, cost)
                heapq.heappush(q[side], (score, cost, tie, next(serial), s, path))
    roots = (start, goal)
    while q[0] or q[1]:
        if time.time() > deadline or time.monotonic() - begin > seconds:
            limited = 'time'
            break
        if sum(expanded) >= cap:
            limited = 'expansions'
            break
        if sum(map(len, seen)) >= 120000:
            limited = 'states'
            break
        side = min((x for x in (0, 1) if q[x]), key=lambda x: expanded[x])
        (_, cost, _, _, s, path) = heapq.heappop(q[side])
        if seen[side][s][0] != cost or cost >= budget:
            continue
        expanded[side] += 1
        for (m, t) in forward(s) if side == 0 else backward(s):
            c = cost + 1
            if c > budget or c >= seen[side].get(t, (100000, None))[0]:
                continue
            route = path + (m,) if side == 0 else (m,) + path
            seen[side][t] = (c, route)
            other = seen[1 - side].get(t)
            if other is not None and c + other[0] <= budget:
                answer = route + other[1] if side == 0 else other[1] + route
                assert len(answer) == c + other[0] and states_of(start, answer)[-1] == goal
                return result(answer)
            answer = bridge(side, t, c, route)
            if answer is not None:
                return result(answer)
            index(side, t)
            if c < budget and c + abs(len(t) - len(roots[1 - side])) <= budget:
                (score, tie) = priority(t, side, c)
                heapq.heappush(q[side], (score, c, tie, next(serial), t, route))
    return result()
