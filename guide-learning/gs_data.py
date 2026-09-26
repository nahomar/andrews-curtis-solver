"""Sample exact-labelled training states from the GS class table (gs22.bin: class -> macro distance to (x, y)).
Each class key packs two cyclic words; we decode a representative (both relators) and label it with its depth."""
import numpy as np, sys, random
sys.path.insert(0, '/Users/nahom/acc/vbias')
from common import SEQ, SEP, PAD
f = open('/Users/nahom/acc/acsolver/gs22.bin', 'rb')
cap, n = np.frombuffer(f.read(16), np.uint64)
keys = np.frombuffer(f.read(16 * int(n)), np.uint64).reshape(-1, 2)
depth = np.frombuffer(f.read(int(n)), np.uint8)
rng = np.random.default_rng(0)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 400000
idx = rng.choice(int(n), N, replace=False)
def unkey(k):
    L = int(k >> np.uint64(58)); w = []
    for i in range(L): w.append(int((k >> np.uint64(2 * (L - 1 - i))) & np.uint64(3)))
    return w
X = np.full((N, SEQ), PAD, np.uint8); Y = np.zeros(N, np.float32)
for j, i in enumerate(idx):
    a, b = unkey(keys[i, 0]), unkey(keys[i, 1])
    if rng.random() < 0.5: a, b = b, a                     # relator order is not canonical in play
    t = a + [SEP] + b; X[j, :len(t)] = t; Y[j] = depth[i]
np.savez('gs_train.npz', X=X, Y=Y)
print('sampled', N, 'classes; depth mean', Y.mean(), 'max', Y.max())
