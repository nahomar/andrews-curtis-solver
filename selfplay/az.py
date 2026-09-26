"""AlphaZero-style self-play prototype for AC trivialisation (elementary moves, 14 actions).

Net: transformer encoder -> (policy logits over 14 moves, value = predicted moves-to-go).
Search: PUCT MCTS guided by the net; exact finish when the state is in a small exact ball.
Self-play: curriculum starts from states k moves before the end of our best verified paths; k grows as the
agent succeeds. Training targets: policy = MCTS visit distribution, value = moves-to-go actually achieved.
    python az.py --iters 20
"""
import argparse, json, math, random, sys, time, numpy as np, torch, torch.nn as nn, torch.nn.functional as F
sys.path.insert(0, '/Users/nahom/acc/vbias')
from common import apply_move, TARGET, SEP, PAD, SEQ, total
DEV = 'mps' if torch.backends.mps.is_available() else 'cpu'
A = '/Users/nahom/acc/acsolver/'

class Net(nn.Module):
    def __init__(self, d=128, layers=4, heads=4):
        super().__init__()
        self.emb = nn.Embedding(6, d); self.pos = nn.Parameter(torch.randn(SEQ, d) * 0.02)
        enc = nn.TransformerEncoderLayer(d, heads, 4 * d, dropout=0.0, batch_first=True, activation='gelu', norm_first=True)
        self.tr = nn.TransformerEncoder(enc, layers, enable_nested_tensor=False)
        self.pol = nn.Sequential(nn.LayerNorm(d), nn.Linear(d, d), nn.GELU(), nn.Linear(d, 14))
        self.val = nn.Sequential(nn.LayerNorm(d), nn.Linear(d, d), nn.GELU(), nn.Linear(d, 1))
    def forward(self, x):
        m = x == PAD; h = self.tr(self.emb(x) + self.pos[: x.shape[1]], src_key_padding_mask=m)
        h = h.masked_fill(m.unsqueeze(-1), 0).sum(1) / (~m).sum(1, keepdim=True).clamp(min=1)
        return self.pol(h), self.val(h).squeeze(-1)

def tok(s):
    t = list(s[0]) + [SEP] + list(s[1])
    return t + [PAD] * (SEQ - len(t)) if len(t) <= SEQ else None

@torch.no_grad()
def evaluate(net, states):
    X = torch.tensor([tok(s) for s in states], dtype=torch.long, device=DEV)
    p, v = net(X); return F.softmax(p, -1).cpu().numpy(), v.clamp(min=0).cpu().numpy()

def legal(s):
    out = []
    for m in range(14):
        t = apply_move(s, m)
        if t != s and t[0] and t[1] and total(t) <= 62: out.append((m, t))
    return out

class Node:
    __slots__ = ('s', 'P', 'N', 'W', 'kids', 'v')
    def __init__(self, s): self.s, self.P, self.N, self.W, self.kids, self.v = s, None, {}, {}, {}, 0.0

def mcts_move(net, root, sims=64, c=1.5, seen=None):
    """Returns (visit distribution over 14 moves, chosen move). Cost = moves, so we minimise v."""
    def expand(node):
        mv = legal(node.s)
        p, v = evaluate(net, [node.s]); node.v = float(v[0])
        node.P = {m: float(p[0][m]) for m, _ in mv}; tot = sum(node.P.values()) or 1
        for m in node.P: node.P[m] /= tot; node.N[m] = 0; node.W[m] = 0.0
        node.kids = {m: t for m, t in mv}
        return node.v
    if root.P is None: expand(root)
    cache = {}
    for _ in range(sims):
        path, node = [], root; onpath = {root.s}
        while True:
            if node.s == TARGET: leaf = 0.0; break
            if node.P is None: leaf = expand(node); break
            tn = sum(node.N.values()) + 1
            best, bm = None, None
            for m, pr in node.P.items():
                if (seen and node.kids[m] in seen) or node.kids[m] in onpath: continue   # no cycles within a simulation
                q = (node.W[m] / node.N[m]) if node.N[m] else node.v + 1          # mean moves-to-go through m
                u = -q + c * pr * math.sqrt(tn) / (1 + node.N[m])
                if best is None or u > best: best, bm = u, m
            if bm is None: leaf = node.v + 10; break
            path.append((node, bm))
            k = node.kids[bm]
            child = cache.get(k)
            if child is None: child = Node(k); cache[k] = child
            node = child; onpath.add(k)
        g = leaf
        for n, m in reversed(path):
            g = g + 1; n.N[m] += 1; n.W[m] += g
    vis = np.zeros(14, np.float32)
    for m, n in root.N.items(): vis[m] = n
    return vis / max(vis.sum(), 1), int(np.argmax(vis))

def play(net, start, limit, sims):
    s, traj, seen = start, [], {start}
    for t in range(limit):
        if s == TARGET: break
        root = Node(s); pi, m = mcts_move(net, root, sims, seen=seen)
        traj.append((s, pi)); s = apply_move(s, m); seen.add(s)
    return traj, s == TARGET

def curriculum_starts(k, n, rng):
    """States k moves before the end of our best near-record paths."""
    rec = json.load(open(A + 'records.json')); db = json.load(open(A + 'best_ac.json'))
    LET = {1: 0, -1: 1, 2: 2, -2: 3}; P = {}
    for l in open(A + 'problems_ac.txt'):
        i, rest = l.split(None, 1); a, b = rest.split('|')
        P[i] = tuple(bytes(LET[int(g)] for g in part.split()) for part in (a, b))
    ids = [i for i, v in db.items() if rec[i][0] and v['length'] <= rec[i][0] + 3 and v['length'] > k]
    out = []
    for i in rng.sample(ids, min(n, len(ids))):
        mv = db[i]['moves']; s = P[i]
        for m in mv[: len(mv) - k]: s = apply_move(s, m)
        if tok(s) is not None: out.append((s, k))            # our path needs exactly k more moves from here
    return out

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('--iters', type=int, default=20); ap.add_argument('--games', type=int, default=24)
    ap.add_argument('--sims', type=int, default=48); ap.add_argument('--k0', type=int, default=6); ap.add_argument('--boot', type=int, default=3000); a = ap.parse_args()
    rng = random.Random(0); net = Net().to(DEV); opt = torch.optim.AdamW(net.parameters(), lr=3e-4, weight_decay=0.01)
    buf, k = [], a.k0
    # bootstrap: imitate our near-record paths (policy = move taken, value = moves remaining)
    rec = json.load(open(A + 'records.json')); db = json.load(open(A + 'best_ac.json'))
    LET = {1: 0, -1: 1, 2: 2, -2: 3}; P = {}
    for l in open(A + 'problems_ac.txt'):
        i, rest = l.split(None, 1); aa, bb = rest.split('|')
        P[i] = tuple(bytes(LET[int(g)] for g in part.split()) for part in (aa, bb))
    demo = []
    for i, v in db.items():
        if not rec[i][0] or v['length'] > rec[i][0] + 3: continue
        mv = v['moves']; s0 = P[i]; L = len(mv)
        for t, m in enumerate(mv):
            tk = tok(s0)
            if tk is not None: demo.append((tk, m, float(L - t)))
            s0 = apply_move(s0, m)
    print(f'bootstrap: {len(demo)} demonstration states', flush=True)
    for step in range(1, a.boot + 1):
        batch = rng.sample(demo, 256)
        X = torch.tensor([b[0] for b in batch], device=DEV); M = torch.tensor([b[1] for b in batch], device=DEV)
        V = torch.tensor([b[2] for b in batch], device=DEV)
        pl, vl = net(X); loss = F.cross_entropy(pl, M) + 0.05 * F.huber_loss(vl, V, delta=4.0)
        opt.zero_grad(); loss.backward(); opt.step()
        if step % 500 == 0:
            acc = (pl.argmax(-1) == M).float().mean().item()
            print(f'  boot step {step} loss {loss.item():.3f} move-acc {acc:.2f}', flush=True)
    torch.save(net.state_dict(), 'az_boot.pt')
    for it in range(1, a.iters + 1):
        t0 = time.time(); starts = curriculum_starts(k, a.games, rng); wins = shorter = 0; lens = []
        for s0, ours in starts:
            traj, ok = play(net, s0, limit=2 * ours + 4, sims=a.sims)
            if ok:
                wins += 1; L = len(traj); lens.append(L - ours); shorter += L < ours
                for t, (s, pi) in enumerate(traj): buf.append((tok(s), pi, float(L - t)))
        buf = buf[-60000:]
        if len(buf) >= 256:
            for _ in range(200):
                batch = rng.sample(buf, 256)
                X = torch.tensor([b[0] for b in batch], device=DEV); PI = torch.tensor(np.stack([b[1] for b in batch]), device=DEV)
                V = torch.tensor([b[2] for b in batch], device=DEV)
                pl, vl = net(X)
                loss = -(PI * F.log_softmax(pl, -1)).sum(-1).mean() + 0.05 * F.huber_loss(vl, V, delta=4.0)
                opt.zero_grad(); loss.backward(); opt.step()
        rate = wins / max(len(starts), 1)
        print(f'iter {it} k={k}: solved {wins}/{len(starts)} ({rate:.0%}), shorter than our path {shorter}, mean diff {np.mean(lens) if lens else float("nan"):+.2f}, buffer {len(buf)}, {time.time()-t0:.0f}s', flush=True)
        if rate >= 0.8: k += 2                                  # curriculum: push the start further from the finish
        torch.save(net.state_dict(), 'az_net.pt')
