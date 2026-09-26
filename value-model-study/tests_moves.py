"""Check common.py against the acc-challenge reference implementation and their verified paths."""
import sys, json, random
sys.path.insert(0, sys.argv[1] if len(sys.argv) > 1 else '../acc-challenge/solver/nn')
import acmoves
from common import apply_move, replay, parse_pool, TARGET, neighbours

rng = random.Random(0)
for _ in range(20000):
    s = tuple(bytes(rng.choice([0, 1, 2, 3]) for _ in range(rng.randint(1, 12))) for _ in range(2))
    s = tuple(acmoves.strip_word(w) or w for w in s)
    for m in range(14):
        assert apply_move(s, m) == acmoves.apply_move(s, m), (s, m)
sol = '../acc-challenge/solver/'
pool, best = parse_pool(sol + 'pool_ac.txt'), json.load(open(sol + 'best_ac.json'))
ends = {replay(pool[i], v['moves'])[-1] for i, v in best.items()}
print('moves agree on 280k checks; distinct path endpoints:', len(ends), sorted(ends)[:3])
