"""Miller-Schupp instances MS(n,w) = <x,y | x^-1 y^n x y^-(n+1), x w^-1> from the official metadata."""
import csv, os, sys
META = os.path.expanduser('~/acc/official/competition/tools/verifier/data/ms1190_metadata.csv')

def reduce(w):
    out = []
    for a in w:
        if out and out[-1] == -a: out.pop()
        else: out.append(a)
    return out

def inv(w): return [-a for a in reversed(w)]

def ms(n, wv):
    r0 = [-1] + [2] * n + [1] + [-2] * (n + 1)
    r1 = reduce([1] + inv(wv))
    return r0, r1

def load(status=None):
    rows = []
    with open(META) as f:
        for r in csv.DictReader(f):
            if status and r['status_at_freeze'] != status: continue
            n = int(r['n']); wv = [int(t) for t in r['w_vector'].split()]
            r0, r1 = ms(n, wv)
            rows.append(dict(seq=int(r['seq']), n=n, w=r['w'], wv=wv, status=r['status_at_freeze'],
                             cls=r['reported_class'], r0=r0, r1=r1, L=len(r0) + len(r1)))
    return rows

if __name__ == '__main__':
    st = sys.argv[1] if len(sys.argv) > 1 else 'open'
    for r in load(st):
        print(f"ms{r['seq']:04d} {' '.join(map(str, r['r0']))} | {' '.join(map(str, r['r1']))}  # n={r['n']} w={r['w']} L={r['L']} class={r['cls']}")
