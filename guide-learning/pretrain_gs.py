"""Pretrain the ValueNet on exact GS class distances (gs_train.npz), save gs_pre.pt."""
import numpy as np, torch, torch.nn.functional as F, sys
sys.path.insert(0, '/Users/nahom/acc/vbias')
from model import ValueNet, save
DEV = 'mps' if torch.backends.mps.is_available() else 'cpu'
d = np.load('gs_train.npz'); X = torch.from_numpy(d['X'].astype(np.int64)); Y = torch.from_numpy(np.minimum(d['Y'], 60))
n = len(Y); va = torch.arange(n - 20000, n); tr = n - 20000
model = ValueNet(128, 4, 4).to(DEV); opt = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=0.01)
g = torch.Generator().manual_seed(0)
for step in range(1, 6001):
    b = torch.randint(0, tr, (512,), generator=g)
    loss = F.huber_loss(model(X[b].to(DEV)), Y[b].to(DEV), delta=4.0)
    opt.zero_grad(); loss.backward(); opt.step()
    if step % 1000 == 0:
        with torch.no_grad():
            p = torch.cat([model(X[va[i:i+4096]].to(DEV)).cpu() for i in range(0, len(va), 4096)])
            yv = Y[va]; mae = (p - yv).abs().mean().item()
            rp, ry = p.argsort().argsort().float(), yv.argsort().argsort().float()
            sp = torch.corrcoef(torch.stack([rp, ry]))[0, 1].item()
        print(f'step {step} loss {loss.item():.3f} | val MAE {mae:.2f} spearman {sp:.3f}', flush=True)
save(model, 'gs_pre.pt'); print('saved gs_pre.pt')
