"""Connect exact states using explicitly reconstructed conjugations."""
from .words import conjugator, word_class
from .graph import within
from .official import apply_move

def key(s):
    return tuple((word_class(w) for w in s))

def connector(a, b, budget):
    if len(a) != len(b):
        return None
    seq = ()
    for (i, (w, v)) in enumerate(zip(a, b)):
        part = conjugator(w, v, i)
        if part is None:
            return None
        seq += part
        if len(seq) > budget:
            return None
    t = a
    for m in seq:
        (t, e) = apply_move(t, m)
        if e or not within(t):
            return None
    assert t == b
    return seq
