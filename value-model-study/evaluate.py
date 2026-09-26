"""Length-bias metrics, reproducing acc-challenge STATUS.md section 3.7 on held-out data.

path  held-out instances: Spearman of score with remaining moves, partial Spearman after removing
      length, and for each step actually taken (which lowers remaining by 1) the mean score change and
      the fraction scored as "worse" (score went up), split by grow / keep / shrink.
sib   held-out path steps: how often the taken move is the model's favourite among all legal moves,
      against the shortest-neighbour rule (the rule gets fractional credit on ties).
ball  held-out exact-ball states: for certified optimal moves (child distance = parent - 1), the fraction
      scored as improving, split by grow / shrink; how often the model's favourite neighbour is optimal
      against the shortest-neighbour rule; how often the favourite is the shortest neighbour.
"""
import argparse, json, numpy as np, torch
from common import SEQ, pad_to, lengths
from data import Ball, Paths

DEV = 'mps' if torch.backends.mps.is_available() else 'cpu'


@torch.no_grad()
def score(model, X, bs=8192):
    out = []
    for i in range(0, len(X), bs):
        out.append(model(torch.from_numpy(pad_to(X[i:i + bs]).astype(np.int64)).to(DEV)).float().cpu())
    return torch.cat(out).numpy() if out else np.zeros(0, np.float32)


def rank(v):
    r = np.empty(len(v)); r[np.argsort(v, kind='stable')] = np.arange(len(v)); return r


def spearman(a, b):
    return float(np.corrcoef(rank(a), rank(b))[0, 1])


def partial_spearman(a, b, c):
    ra, rb, rc = rank(a), rank(b), rank(c)
    res = lambda y: y - np.polyval(np.polyfit(rc, y, 1), rc)
    return float(np.corrcoef(res(ra), res(rb))[0, 1])


def shortest_rule_credit(L_children, good):
    s = L_children == L_children.min()
    return good[s].mean()


def evaluate(model, paths, ball=None, n=20000, n_sib=2000, n_ball=4000, seed=0):
    model.eval(); rng = np.random.default_rng(seed); r = {}
    # path states
    idx = rng.choice(paths.va_states, min(n, len(paths.va_states)), replace=False)
    s, y, L = score(model, paths.X[idx]), paths.Y[idx], paths.L[idx]
    r['path_mae'] = float(np.abs(s - y).mean())
    r['path_spearman'] = spearman(s, y); r['path_len_spearman'] = spearman(L, y)
    r['path_partial'] = partial_spearman(s, y, L)
    # path steps
    t = rng.choice(paths.va_trans, min(n, len(paths.va_trans)), replace=False)
    d = score(model, paths.X[paths.TB[t]]) - score(model, paths.X[paths.TA[t]])
    dl = paths.L[paths.TB[t]].astype(int) - paths.L[paths.TA[t]]
    for name, m in (('grow', dl > 0), ('keep', dl == 0), ('shrink', dl < 0)):
        r[f'step_{name}_dscore'] = float(d[m].mean()); r[f'step_{name}_worse'] = float((d[m] > 0).mean())
    r['step_consistency_mae'] = float(np.abs(d + 1).mean())
    # siblings along held-out paths
    top, rule, top_g, rule_g = [], [], [], []
    for ti in rng.choice(paths.va_trans, min(n_sib, len(paths.va_trans)), replace=False):
        rows, k = paths.siblings(ti)
        sc = score(model, rows); good = np.zeros(len(rows)); good[k] = 1
        hit = float(np.argmin(sc) == k); cr = shortest_rule_credit(lengths(rows), good)
        top.append(hit); rule.append(cr)
        if paths.grow[ti]: top_g.append(hit); rule_g.append(cr)
    r['sib_top1'], r['sib_rule'] = float(np.mean(top)), float(np.mean(rule))
    r['sib_top1_grow'], r['sib_rule_grow'] = float(np.mean(top_g)), float(np.mean(rule_g))
    if ball is None:
        return r
    # exact ball
    cand = np.where(ball.VAL & (ball.D >= 1))[0]
    par = rng.choice(cand, min(n_ball, len(cand)), replace=False)
    ps = score(model, ball.X[par])
    imp_g, imp_s, fav_opt, rule_opt, fav_short, fav_grow = [], [], [], [], [], []
    for p, sp, (_, ts, dc) in zip(par, ps, ball.children(par)):
        from common import encode
        rows = np.frombuffer(b''.join(encode(u, SEQ) for u in ts), np.uint8).reshape(-1, SEQ)
        sc = score(model, rows); Lc = lengths(rows); Lp = lengths(ball.X[p:p + 1])[0]
        opt = dc == ball.D[p] - 1
        for j in np.where(opt)[0]:
            (imp_g if Lc[j] > Lp else imp_s if Lc[j] < Lp else []).append(sc[j] < sp)
        f = np.argmin(sc)
        fav_opt.append(opt[f]); rule_opt.append(shortest_rule_credit(Lc, opt.astype(float)))
        fav_short.append(Lc[f] == Lc.min()); fav_grow.append(Lc[f] > Lp)
    r['ball_opt_grow_improving'] = float(np.mean(imp_g)) if imp_g else float('nan')
    r['ball_opt_shrink_improving'] = float(np.mean(imp_s)) if imp_s else float('nan')
    r['ball_fav_optimal'], r['ball_rule_optimal'] = float(np.mean(fav_opt)), float(np.mean(rule_opt))
    r['ball_fav_shortest'], r['ball_fav_grows'] = float(np.mean(fav_short)), float(np.mean(fav_grow))
    r['ball_mae'] = float(np.abs(ps - ball.D[par]).mean())
    return r


def show(r):
    order = ['path_mae', 'path_spearman', 'path_len_spearman', 'path_partial',
             'step_grow_dscore', 'step_grow_worse', 'step_keep_dscore', 'step_shrink_dscore', 'step_shrink_worse', 'step_consistency_mae',
             'sib_top1', 'sib_rule', 'sib_top1_grow', 'sib_rule_grow',
             'ball_mae', 'ball_opt_grow_improving', 'ball_opt_shrink_improving', 'ball_fav_optimal', 'ball_rule_optimal', 'ball_fav_shortest', 'ball_fav_grows']
    for k in order:
        if k in r: print(f'  {k:28s} {r[k]:+.3f}')


if __name__ == '__main__':
    from model import load
    ap = argparse.ArgumentParser()
    ap.add_argument('--model', required=True); ap.add_argument('--ball', default='data/ball12.npz')
    ap.add_argument('--out', default='')
    a = ap.parse_args()
    r = evaluate(load(a.model, DEV), Paths('data/paths.npz'), Ball(a.ball) if a.ball else None)
    show(r)
    if a.out: json.dump(r, open(a.out, 'w'), indent=1)
