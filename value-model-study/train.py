"""Train a value model with the baseline loss plus any of the untried fixes.

Baseline (acc-challenge train_value.py): Huber regression of remaining moves on path + ball states.
  --w-step     step consistency: on path transitions, v(s_t) - v(s_t+1) should be 1
  --grow-w     upweight path states / transitions whose path step grows the presentation
  --w-rank-ball pairwise ranking of sibling children in the exact ball (lower distance should score lower)
  --w-rank-path listwise ranking on paths: the move actually taken should be the favourite among siblings
  --residual   predict the residual over a least-squares length baseline
"""
import argparse, json, os, time, numpy as np, torch, torch.nn as nn, torch.nn.functional as F
from common import SEQ, encode, pad_to, lengths
from data import Ball, Paths
from model import ValueNet, save
from evaluate import evaluate, show, DEV

ap = argparse.ArgumentParser()
ap.add_argument('--tag', required=True)
ap.add_argument('--ball', default='data/ball12.npz')
ap.add_argument('--steps', type=int, default=6000); ap.add_argument('--bs', type=int, default=1024)
ap.add_argument('--ball-frac', type=float, default=0.4); ap.add_argument('--lr', type=float, default=1e-3)
ap.add_argument('--d', type=int, default=128); ap.add_argument('--layers', type=int, default=4)
ap.add_argument('--w-step', type=float, default=0.0); ap.add_argument('--step-bs', type=int, default=512)
ap.add_argument('--grow-w', type=float, default=1.0)
ap.add_argument('--w-rank-ball', type=float, default=0.0); ap.add_argument('--rank-ball-bs', type=int, default=64)
ap.add_argument('--w-rank-path', type=float, default=0.0); ap.add_argument('--rank-path-bs', type=int, default=32)
ap.add_argument('--tau', type=float, default=0.5)
ap.add_argument('--residual', action='store_true')
ap.add_argument('--eval-every', type=int, default=2000); ap.add_argument('--seed', type=int, default=0)
a = ap.parse_args()
torch.manual_seed(a.seed); rng = np.random.default_rng(a.seed)
out = f'runs/{a.tag}'; os.makedirs(out, exist_ok=True)
json.dump(vars(a), open(f'{out}/args.json', 'w'), indent=1)

P, B = Paths('data/paths.npz'), Ball(a.ball)
b_tr = np.where(~B.VAL)[0]; b_par = np.where(~B.VAL & (B.D >= 1))[0]
Xp = torch.from_numpy(P.X.astype(np.int64)); Yp = torch.from_numpy(P.Y)
Xb = torch.from_numpy(pad_to(B.X).astype(np.int64)); Yb = torch.from_numpy(B.D)
Wp = torch.from_numpy(np.where(P.state_grows, a.grow_w, 1.0).astype(np.float32))

residual = None
if a.residual:   # least-squares y ~ a*len + b on the same path/ball mixture the regression loss sees
    ip, ib = rng.choice(P.tr_states, 50000), rng.choice(b_tr, int(50000 * a.ball_frac / (1 - a.ball_frac)))
    Lm = np.concatenate([P.L[ip], lengths(B.X[ib])]); Ym = np.concatenate([P.Y[ip], B.D[ib]])
    residual = tuple(float(v) for v in np.polyfit(Lm, Ym, 1)); print('residual baseline a, b =', residual)
model = ValueNet(a.d, a.layers, 4, residual=residual).to(DEV)
opt = torch.optim.AdamW(model.parameters(), lr=a.lr, weight_decay=0.01)
sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=a.lr, total_steps=a.steps, pct_start=0.05)
huber = lambda p, y, w: (F.huber_loss(p, y, delta=4.0, reduction='none') * w).sum() / w.sum()

nb = int(a.bs * a.ball_frac); npth = a.bs - nb
log = open(f'{out}/log.txt', 'w'); t0 = time.time(); run = {}
for step in range(1, a.steps + 1):
    ip = P.tr_states[rng.integers(0, len(P.tr_states), npth)]; ib = b_tr[rng.integers(0, len(b_tr), nb)]
    x = torch.cat([Xp[ip], Xb[ib]]).to(DEV); y = torch.cat([Yp[ip], Yb[ib]]).to(DEV)
    w = torch.cat([Wp[ip], torch.ones(nb)]).to(DEV)
    losses = {'reg': huber(model(x), y, w)}
    if a.w_step:
        t = P.tr_trans[rng.integers(0, len(P.tr_trans), a.step_bs)]
        va, vb = model(Xp[P.TA[t]].to(DEV)), model(Xp[P.TB[t]].to(DEV))
        tw = torch.from_numpy(np.where(P.grow[t], a.grow_w, 1.0).astype(np.float32)).to(DEV)
        losses['step'] = a.w_step * huber(va - vb, torch.ones_like(va), tw)
    if a.w_rank_ball:
        par = b_par[rng.integers(0, len(b_par), a.rank_ball_bs)]
        rows, grp, dist = [], [], []
        for g, (_, ts, dc) in enumerate(B.children(par)):
            for u, dd in zip(ts, dc):
                rows.append(encode(u)); grp.append(g); dist.append(dd)
        v = model(torch.from_numpy(np.frombuffer(b''.join(rows), np.uint8).reshape(-1, SEQ).astype(np.int64)).to(DEV))
        grp, dist = torch.tensor(grp, device=DEV), torch.tensor(dist, device=DEV)
        pair = (grp[:, None] == grp[None, :]) & (dist[:, None] < dist[None, :])   # i should score below j
        losses['rank_ball'] = a.w_rank_ball * F.softplus((v[:, None] - v[None, :]) / a.tau)[pair].mean()
    if a.w_rank_path:
        ts = P.tr_trans[rng.integers(0, len(P.tr_trans), a.rank_path_bs)]
        sib = [P.siblings(t) for t in ts]
        v = model(torch.from_numpy(np.concatenate([r for r, _ in sib]).astype(np.int64)).to(DEV))
        ce, k0 = [], 0
        for (r, k), t in zip(sib, ts):
            ce.append(F.cross_entropy((-v[k0:k0 + len(r)] / a.tau)[None], torch.tensor([k], device=DEV)) * (a.grow_w if P.grow[t] else 1.0))
            k0 += len(r)
        losses['rank_path'] = a.w_rank_path * torch.stack(ce).mean()
    loss = sum(losses.values())
    opt.zero_grad(set_to_none=True); loss.backward(); nn.utils.clip_grad_norm_(model.parameters(), 1.0); opt.step(); sched.step()
    for k, v in losses.items():
        run[k] = 0.98 * run.get(k, v.item()) + 0.02 * v.item()
    if step % 200 == 0:
        msg = f'step {step} ' + ' '.join(f'{k} {v:.3f}' for k, v in run.items()) + f' {time.time() - t0:.0f}s'
        print(msg, flush=True); log.write(msg + '\n'); log.flush()
    if step % a.eval_every == 0 or step == a.steps:
        r = evaluate(model, P, B); model.train()
        print(f'eval @ {step}', flush=True); show(r)
        json.dump(r, open(f'{out}/eval.json', 'w'), indent=1); save(model, f'{out}/model.pt')
