"""Word heuristics and paid conjugation witnesses."""
import functools
import itertools
from functools import lru_cache
from .official import MOVE_TABLE, free_reduce

@functools.lru_cache(maxsize=60000)
def lev(a, b):
    if not a:
        return len(b)
    mask = (1 << len(a)) - 1
    pv = mask
    mv = 0
    score = len(a)
    high = 1 << len(a) - 1
    eq = {}
    for (i, c) in enumerate(a):
        eq[c] = eq.get(c, 0) | 1 << i
    for c in b:
        e = eq.get(c, 0)
        xv = e | mv
        xh = (e & pv) + pv ^ pv | e
        ph = mv | ~(xh | pv)
        mh = pv & xh
        score += bool(ph & high) - bool(mh & high)
        ph = ph << 1 | 1
        mh <<= 1
        pv = (mh | ~(xv | ph)) & mask
        mv = ph & xv
    return score

def aligned(s, g):
    return 2 * abs(len(s) - len(g)) + sum((lev(a, b) for (a, b) in itertools.zip_longest(s, g, fillvalue=())))

@lru_cache(maxsize=100000)
def word_class(word):
    while len(word) > 1 and word[0] == -word[-1]:
        word = word[1:-1]
    inverse = tuple((-x for x in word[::-1]))
    return min([w[i:] + w[:i] for w in (word, inverse) for i in range(len(w))] or [()])

def inverse(w):
    return tuple((-x for x in w[::-1]))

def product_length(a, b):
    k = 0
    while k < min(len(a), len(b)) and a[-k - 1] == -b[k]:
        k += 1
    return len(a) + len(b) - 2 * k

def product(s, g):
    return 2 * abs(len(s) - len(g)) + sum((min(product_length(a, inverse(b)), product_length(inverse(b), a)) for (a, b) in itertools.zip_longest(s, g, fillvalue=())))

INV = {r['relator']: r['id'] for r in MOVE_TABLE if r['category'] == 'inversion'}

CONJ = {(r['relator'], r['conjugator']): r['id'] for r in MOVE_TABLE if r['category'] == 'conjugation'}

def peel(w):
    p = []
    while len(w) > 1 and w[0] == -w[-1]:
        p.append(w[0])
        w = w[1:-1]
    return (tuple(p), w)

@functools.lru_cache(maxsize=30000)
def conjugator(source, target, index):
    if source == target:
        return ()
    (q, d) = peel(target)
    best = None
    for sign in [False, True]:
        (p, c) = peel(inverse(source) if sign else source)
        if not c or len(c) != len(d):
            continue
        for i in range(len(c)):
            if c[i:] + c[:i] != d:
                continue
            for centralizer in [(), c, inverse(c)]:
                a = tuple(free_reduce(q + inverse(c[:i]) + centralizer + inverse(p)))
                seq = ((INV[index],) if sign else ()) + tuple((CONJ[index, x] for x in reversed(a)))
                if best is None or (len(seq), seq) < (len(best), best):
                    best = seq
    return best
