"""Dataset access for training and evaluation, including on-the-fly neighbour expansion."""
import numpy as np
from common import SEQ, SEP, PAD, TARGET, neighbours, encode, pad_to, lengths


def decode(row):
    row = bytes(row); k = row.index(SEP)
    return row[:k], row[k + 1:].split(bytes([PAD]))[0]


class Ball:
    """Exact ball. Lookup of arbitrary states by binary search over sorted fixed-width keys."""

    def __init__(self, path, val_frac=0.05, seed=0):
        z = np.load(path)
        X, D = z['X'], z['D']
        self.w = X.shape[1]; self.cap = self.w - 1
        keys = (X + 1).view(f'S{self.w}').ravel()     # +1 so letter 0 is never a trailing NUL
        order = np.argsort(keys)
        self.keys, self.X, self.D = keys[order], X[order], D[order].astype(np.float32)
        self.VAL = np.random.default_rng(seed).random(len(self.D)) < val_frac

    def lookup(self, states):
        """Distance of each state, or -1 if it is outside the ball."""
        out = np.full(len(states), -1.0, np.float32)
        rows = [encode(s, self.w) for s in states]
        ok = [k for k, r in enumerate(rows) if r is not None]
        if not ok:
            return out
        q = (np.frombuffer(b''.join(rows[k] for k in ok), np.uint8).reshape(-1, self.w) + 1).view(f'S{self.w}').ravel()
        pos = np.searchsorted(self.keys, q).clip(max=len(self.keys) - 1)
        hit = self.keys[pos] == q
        out[np.array(ok)[hit]] = self.D[pos[hit]]
        return out

    def children(self, idx):
        """For each ball state: its in-ball neighbours (moves, token rows, exact distances)."""
        res = []
        for i in idx:
            s = decode(self.X[i])
            nb = neighbours(s, self.cap)
            d = self.lookup([t for _, t in nb])
            keep = d >= 0
            res.append((np.array([m for m, _ in nb])[keep], [t for (_, t), k in zip(nb, keep) if k], d[keep]))
        return res


class Paths:
    def __init__(self, path, ymax=200):
        z = np.load(path)
        self.X, self.Y, self.G, self.VAL = z['X'], z['Y'], z['G'], z['VAL']
        self.TA, self.TB, self.TM = z['TA'], z['TB'], z['TM']
        self.L = lengths(self.X)
        self.ok = self.Y <= ymax
        real = (self.X[self.TA] != self.X[self.TB]).any(1)          # drop no-op moves on the paths
        tv = self.VAL[self.TA]
        tok = self.ok[self.TA] & real
        self.tr_states = np.where(~self.VAL & self.ok)[0]
        self.va_states = np.where(self.VAL & self.ok)[0]
        self.tr_trans = np.where(~tv & tok)[0]
        self.va_trans = np.where(tv & tok)[0]
        self.grow = self.L[self.TB] > self.L[self.TA]            # per transition
        self.state_grows = np.zeros(len(self.X), bool)           # outgoing path step grows
        self.state_grows[self.TA[self.grow]] = True

    def siblings(self, t):
        """All legal next states from TA[t]; returns (token rows, index of the move actually taken)."""
        s = decode(self.X[self.TA[t]])
        nb = neighbours(s, SEQ - 1)
        rows = [encode(u) for _, u in nb]
        moves = [m for m, _ in nb]
        taken = moves.index(int(self.TM[t]))
        keep = [k for k, r in enumerate(rows) if r is not None]
        return np.frombuffer(b''.join(rows[k] for k in keep), np.uint8).reshape(-1, SEQ), keep.index(taken)
