import torch, torch.nn as nn
from common import SEQ, PAD, SEP


class ValueNet(nn.Module):
    """Same shape as acc-challenge's scorer: pre-LN transformer encoder over r0 SEP r1, mean-pooled,
    predicts remaining moves. With residual=(a, b) the net predicts y - (a * length + b) and the
    length baseline is added back, so the net only has to learn what length misses."""

    def __init__(self, d=128, layers=4, heads=4, seq=SEQ, residual=None):
        super().__init__()
        self.cfg = dict(d=d, layers=layers, heads=heads, seq=seq, residual=residual)
        self.emb = nn.Embedding(6, d)
        self.pos = nn.Parameter(torch.randn(seq, d) * 0.02)
        enc = nn.TransformerEncoderLayer(d, heads, 4 * d, dropout=0.0, batch_first=True, activation='gelu', norm_first=True)
        self.tr = nn.TransformerEncoder(enc, layers, enable_nested_tensor=False)
        self.head = nn.Sequential(nn.LayerNorm(d), nn.Linear(d, d), nn.GELU(), nn.Linear(d, 1))
        self.residual = residual

    def forward(self, x):
        mask = x == PAD
        h = self.tr(self.emb(x) + self.pos[: x.shape[1]], src_key_padding_mask=mask)
        h = h.masked_fill(mask.unsqueeze(-1), 0).sum(1) / (~mask).sum(1, keepdim=True).clamp(min=1)
        out = self.head(h).squeeze(-1)
        if self.residual is not None:
            a, b = self.residual
            out = out + a * (x < SEP).sum(1).float() + b
        return out


def save(model, path):
    torch.save({'cfg': model.cfg, 'sd': model.state_dict()}, path)


def load(path, device='cpu'):
    ck = torch.load(path, map_location='cpu')
    m = ValueNet(**ck['cfg']); m.load_state_dict(ck['sd'])
    return m.to(device).eval()
