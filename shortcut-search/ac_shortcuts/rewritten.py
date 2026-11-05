"""Compose original and universally rewritten waypoint seeds within one query budget."""
import time
from .composition import compose
from .native import search as native_search, lev, word_class
from .words import conjugator
from .graph import states_of, within
from .rewrite import rewrite

def schedule(length):
    candidates = [(0, length), (0, length // 2), (length // 2, length), (length // 4, 3 * length // 4)]
    return list(dict.fromkeys(((i, j) for (i, j) in candidates if j - i >= 2)))

def search(start, goal, old_moves, budget, cap, deadline, seconds=1):
    begin = time.monotonic()
    old = tuple(old_moves)
    states = states_of(start, old)
    assert states[-1] == goal and 0 <= budget < len(old)
    windows = [(i, j) for (i, j) in schedule(len(old)) if within(states[i]) and within(states[j])]
    (edges, attempts) = ([], [])
    best = compose(start, old, edges)
    expanded = 0
    limited = None
    for (index, (i, j)) in enumerate(windows):
        remaining = min(seconds - (time.monotonic() - begin), deadline - time.time())
        if remaining <= 0:
            limited = 'time'
            break
        if expanded >= cap:
            limited = 'expansions'
            break
        allowance = remaining / (len(windows) - index)
        slot_start = time.monotonic()
        lev.cache_clear()
        word_class.cache_clear()
        conjugator.cache_clear()
        allowance -= time.monotonic() - slot_start
        if allowance <= 0:
            attempts.append(dict(from_index=i, to_index=j, budget=j - i - 1, moves=None, expanded=0, limited='setup-time'))
            continue
        rewrite_begin = time.monotonic()
        variant = rewrite(states[i], states[j], old[i:j], j - i - 1, cap - expanded, deadline, seconds=max(0, allowance * 0.1))
        allowance -= time.monotonic() - rewrite_begin
        if variant['moves'] is not None:
            result = variant
        elif allowance > 0 and cap - expanded > variant['expanded']:
            alternatives = [variant['best_local_moves']] if tuple(variant['best_local_moves']) != old[i:j] else []
            result = native_search('native_shared', states[i], states[j], old[i:j], j - i - 1, cap - expanded - variant['expanded'], deadline, seconds=allowance, extra_paths=alternatives)
            result['expanded'] += variant['expanded']
        else:
            result = dict(moves=None, expanded=variant['expanded'], seconds=time.monotonic() - rewrite_begin, limited='rewrite-resource-limit')
        result['rewrite_stats'] = {k: variant[k] for k in ['rewrites', 'atomic_saving', 'expanded', 'seconds']}
        expanded += result['expanded']
        attempts.append(dict(result, from_index=i, to_index=j, budget=j - i - 1))
        if result['moves'] is not None:
            edges.append(dict(from_index=i, to_index=j, moves=result['moves']))
            best = compose(start, old, edges)
            if best['length'] <= budget:
                break
    success = best['length'] <= budget
    return dict(moves=best['moves'] if success else None, best_local_moves=best['moves'], atomic_saving=best['atomic_saving'], selected_shortcut_indices=best['selected_shortcut_indices'], strict_budget=budget, success=success, expanded=expanded, seconds=time.monotonic() - begin, limited=limited, windows=windows, attempts=attempts, verified_edges=edges)
