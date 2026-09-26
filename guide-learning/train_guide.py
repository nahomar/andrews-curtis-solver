"""Train a search guide on labelled decision states (neighbours of best-path states).

Label = length of the solution a short search found from that state (unsolved -> censored at worst+10).
Loss = Huber regression + listwise ranking within each sibling group (the choice the beam makes).
Eval (held-out puzzles): how often the model's favourite sibling is the truly best one, vs the shortest-first rule.
    python train_guide.py --labels ../acsolver/results/acc1_label1_ac.jsonl --meta meta_60000.jsonl
"""
import argparse, collections, json, random, sys, numpy as np, torch, torch.nn.functional as F
sys.path.insert(0, '/Users/nahom/acc/vbias')
from model import ValueNet, save
from common import SEQ, SEP, PAD
ap = argparse.ArgumentParser()
ap.add_argument('--labels', required=True, nargs='+'); ap.add_argument('--meta', required=True)
ap.add_argument('--steps', type=int, default=4000); ap.add_argument('--init', default=''); ap.add_argument('--out', default='guide1.pt'); ap.add_argument('--tau', type=float, default=2.0)
a = ap.parse_args()
DEV = 'mps' if torch.backends.mps.is_available() else 'cpu'
lab = {}
for f in a.labels:                     # several passes: keep the shortest solution found for each state
    for l in open(f):
        try: r = json.loads(l)
        except ValueError: continue          # skip a garbled line
        y = r['length'] if r['solved'] else -1
        if y >= 0 and (lab.get(r['id'], -1) < 0 or y < lab[r['id']]): lab[r['id']] = y
        elif r['id'] not in lab: lab[r['id']] = -1
groups = collections.defaultdict(list)
for l in open(a.meta):
    m = json.loads(l)
    if m['id'] in lab: groups[(m['puzzle'], m['t'])].append((m, lab[m['id']]))
def tok(r0, r1):
    t = r0 + [SEP] + r1
    return t + [PAD] * (SEQ - len(t)) if len(t) <= SEQ else None
G = []
for key, rows in groups.items():
    solved = [y for _, y in rows if y >= 0]
    if len(rows) < 3 or len(solved) < 2: continue
    worst = max(solved) + 10
    X = [tok(m['r0'], m['r1']) for m, _ in rows]
    if any(x is None for x in X): continue
    Y = [y if y >= 0 else worst for _, y in rows]
    L = [len(m['r0']) + len(m['r1']) for m, _ in rows]
    G.append((key[0], np.array(X, np.int64), np.array(Y, np.float32), np.array(L)))
puz = sorted({g[0] for g in G}); random.Random(0).shuffle(puz); val = set(puz[: len(puz) // 5])
tr = [g for g in G if g[0] not in val]; va = [g for g in G if g[0] in val]
print(f'{len(G)} sibling groups ({len(tr)} train / {len(va)} val), device {DEV}', flush=True)
model = ValueNet(128, 4, 4).to(DEV)
if a.init: model.load_state_dict(torch.load(a.init, map_location='cpu')['sd']); print('initialised from', a.init)
opt = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=0.01)
def evaluate():
    model.eval(); hit = rule = 0.0
    with torch.no_grad():
        for _, X, Y, L in va:
            v = model(torch.from_numpy(X).to(DEV)).cpu().numpy()
            best = Y == Y.min()
            hit += best[np.argmin(v)]; s = L == L.min(); rule += best[s].mean()
    model.train(); return hit / len(va), rule / len(va)
rng = random.Random(1)
for step in range(1, a.steps + 1):
    batch = rng.sample(tr, 32); loss = 0
    for _, X, Y, L in batch:
        v = model(torch.from_numpy(X).to(DEV)); y = torch.from_numpy(Y).to(DEV)
        loss = loss + F.huber_loss(v, y, delta=4.0) + F.cross_entropy((-v / a.tau)[None], F.softmax(-y / a.tau, 0)[None])
    loss = loss / len(batch)
    opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0); opt.step()
    if step % 500 == 0:
        h, r = evaluate(); print(f'step {step} loss {loss.item():.3f} | val: model picks a best sibling {h:.3f} vs shortest-first rule {r:.3f}', flush=True)
save(model, a.out); print('saved', a.out)
