"""Public API for shortening a supplied path between exact presentations."""
import math
import threading
import time
from .official import free_reduce
from .graph import states_of, within
from . import native, rewritten
from .words import conjugator

_LOCK = threading.RLock()


def shorten(start, old_moves, *, budget=None, engine='rewritten', seconds=1.0,
            expansion_cap=1_000_000):
    """Return a replayed shortcut, or ``moves=None`` after bounded search.

    ``start`` is a sequence of freely reduced integer words. ``old_moves``
    uses official sac-r8-v1 IDs. The endpoint is determined by replaying that
    witness; it need not be the empty presentation. Every returned move costs
    one. Calls are serialized because the native adapters temporarily replace
    word kernels. This is a heuristic search, not an optimality certificate.
    """
    if engine not in ('shared', 'rewritten'):
        raise ValueError('engine must be shared or rewritten')
    if isinstance(seconds, bool) or not isinstance(seconds, (float, int)) or not math.isfinite(seconds) or seconds <= 0:
        raise ValueError('seconds must be a positive finite number')
    if type(expansion_cap) is not int or expansion_cap < 1:
        raise ValueError('expansion_cap must be a positive integer')
    try:
        start = tuple(tuple(word) for word in start)
        old_moves = tuple(old_moves)
    except TypeError as error:
        raise ValueError('Expected a sequence of words and a sequence of move IDs') from error
    if not within(start) or any(type(x) is not int or not 1 <= abs(x) <= len(start) for word in start for x in word):
        raise ValueError('Expected rank <=3, word length <=48, total length <=80, and valid generator IDs')
    if any(tuple(free_reduce(word)) != word for word in start):
        raise ValueError('Input words must be freely reduced')
    if any(type(move) is not int for move in old_moves):
        raise ValueError('Move IDs must be integers')
    if budget is None:
        budget = max(0, len(old_moves) - 1)
    if type(budget) is not int or budget < 0 or (old_moves and budget >= len(old_moves)):
        raise ValueError('budget must be nonnegative and strictly shorter than the witness')
    trace = states_of(start, old_moves)
    if not all(within(state) for state in trace):
        raise ValueError('The supplied witness leaves the supported state bounds')
    goal = trace[-1]
    with _LOCK:
        began = time.monotonic()
        deadline = time.time() + seconds
        native.lev.cache_clear()
        native.word_class.cache_clear()
        conjugator.cache_clear()
        if start == goal:
            result = dict(moves=(), expanded=0, limited=None, seconds=0)
        elif engine == 'shared':
            result = native.search('native_shared', start, goal, old_moves, budget,
                                   expansion_cap, deadline, seconds=seconds)
        else:
            result = rewritten.search(start, goal, old_moves, budget, expansion_cap,
                                      deadline, seconds=seconds)
        moves = result['moves']
        if moves is not None:
            moves = tuple(moves)
            if len(moves) > budget or states_of(start, moves)[-1] != goal:
                raise RuntimeError('Search produced an invalid shortcut')
            result['moves'] = moves
        result.update(engine=engine, backend=native.BACKEND, old_length=len(old_moves),
                      budget=budget, found_length=None if moves is None else len(moves),
                      atomic_saving=None if moves is None else len(old_moves) - len(moves),
                      total_seconds=time.monotonic() - began)
        return result
