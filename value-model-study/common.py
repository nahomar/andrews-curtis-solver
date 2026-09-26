"""AC moves on 2-relator presentations, tokenisation and the small helpers every script shares.

Letters: 0=x 1=X 2=y 3=Y (inverse = letter ^ 1). A state is (r0, r1) as bytes, freely reduced but not
cyclically reduced. Move numbering matches the acc-challenge C++ solver so their verified paths replay:
  0/1   invert r0 / r1
  2/3   r0 <- r0 r1 / r0 r1^-1        4/5   r1 <- r1 r0 / r1 r0^-1
  6-9   r0 <- c r0 c^-1 for c = m-6   10-13 r1 <- c r1 c^-1 for c = m-10
"""
import numpy as np

SEP, PAD = 4, 5
SEQ = 66                       # r0 + SEP + r1 + pad, total relator length <= 64
TARGET = (bytes([0]), bytes([2]))
INV_MOVE = [0, 1, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12]
_INV = bytes.maketrans(bytes([0, 1, 2, 3]), bytes([1, 0, 3, 2]))


def invert(w):
    return w[::-1].translate(_INV)


def mul(a, b):
    i, j = len(a), 0
    while i and j < len(b) and a[i - 1] ^ 1 == b[j]:
        i -= 1; j += 1
    return a[:i] + b[j:]


def conj(w, c):
    if not w:
        return w
    w = w[1:] if w[0] == c ^ 1 else bytes([c]) + w
    return w[:-1] if w and w[-1] == c else w + bytes([c ^ 1])


def apply_move(s, m):
    r0, r1 = s
    if m == 0: return invert(r0), r1
    if m == 1: return r0, invert(r1)
    if m == 2: return mul(r0, r1), r1
    if m == 3: return mul(r0, invert(r1)), r1
    if m == 4: return r0, mul(r1, r0)
    if m == 5: return r0, mul(r1, invert(r0))
    if m < 10: return conj(r0, m - 6), r1
    return r0, conj(r1, m - 10)


def total(s):
    return len(s[0]) + len(s[1])


def neighbours(s, cap=64):
    """(move, state) for every move that changes s, keeps both relators non-empty and total length <= cap."""
    out = []
    for m in range(14):
        t = apply_move(s, m)
        if t != s and t[0] and t[1] and total(t) <= cap:
            out.append((m, t))
    return out


def replay(s, moves):
    out = [s]
    for m in moves:
        s = apply_move(s, m); out.append(s)
    return out


def parse_pool(path):
    """acc-challenge pool format: `id  a b c | d e f` with generators 1=x 2=y, negative = inverse."""
    pool = {}
    for line in open(path):
        if not line.strip():
            continue
        i, rest = line.split(None, 1)
        pool[i] = tuple(bytes((abs(int(g)) - 1) * 2 + (int(g) < 0) for g in part.split()) for part in rest.split('|'))
    return pool


def encode(s, width=SEQ):
    """Token row r0 SEP r1 PAD..., or None if it does not fit."""
    n = total(s) + 1
    if n > width:
        return None
    return s[0] + bytes([SEP]) + s[1] + bytes([PAD]) * (width - n)


def encode_many(states, width=SEQ):
    rows = [encode(s, width) for s in states]
    ok = np.array([r is not None for r in rows])
    X = np.frombuffer(b''.join(r for r in rows if r is not None), np.uint8).reshape(-1, width)
    return X, ok


def pad_to(X, width=SEQ):
    if X.shape[1] == width:
        return X
    out = np.full((len(X), width), PAD, np.uint8); out[:, :X.shape[1]] = X
    return out


def lengths(X):
    return (X < SEP).sum(1)
