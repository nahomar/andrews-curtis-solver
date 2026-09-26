"""Pipeline helpers around the verified DBs (best_ac.json / best_sac.json from check.py).

  python3 tools.py sac-from-ac               AC DB paths + [16, 15] -> results/sac_from_ac.jsonl (feed to check.py --db best_sac.json)
  python3 tools.py near-misses GMAX OUT.txt  problems whose DB path is 1..GMAX above the record (AC and SAC), for reruns
  python3 tools.py unsolved OUT.txt          problems with a record but no DB path yet
  python3 tools.py plan [OUTDIR]             submission files (<= 500 lines each) with every DB path that beats or ties
                                             the record, best expected gain first; prints the expected score change

records.json must hold the latest official snapshot: id -> [ac_best, ac_k, sac_best, sac_k].
"""
import json, os, sys
rec = json.load(open('records.json'))
submitted = json.load(open('submitted.json')) if os.path.exists('submitted.json') else {}
def load(f): return json.load(open(f)) if os.path.exists(f) else {}
def base(i): return 'ac-' + i.split('-')[1]
def record(i):
    r = rec[base(i)]
    return (r[0], r[1]) if i.startswith('ac-') else (r[2], r[3])
def problems():
    P = {}
    for f in ('problems_ac.txt', 'problems_sac.txt'):
        for l in open(f): P[l.split()[0]] = l
    return P

cmd = sys.argv[1]
if cmd == 'sac-from-ac':
    ac = load('best_ac.json'); n = 0
    with open('results/sac_from_ac.jsonl', 'w') as f:
        for i, v in ac.items():
            mv = v['moves'] + [16, 15]
            f.write(json.dumps({'id': 'sac-' + i.split('-')[1], 'solved': True, 'length': len(mv), 'moves': mv}) + '\n'); n += 1
    print(n, 'SAC candidates -> results/sac_from_ac.jsonl')
elif cmd == 'near-misses':
    G, out = int(sys.argv[2]), sys.argv[3]; RMAX = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9; P = problems(); rows = []
    for db in ('best_ac.json', 'best_sac.json'):
        for i, v in load(db).items():
            R, k = record(i)
            if R and R < RMAX and 1 <= v['length'] - R <= G: rows.append((v['length'] - R, i))
    rows.sort(key=lambda r: -(record(r[1])[0] or 0))    # longest records first: slow jobs start early, short ones fill the tail
    open(out, 'w').writelines(P[i] for _, i in rows)
    print(len(rows), 'near misses ->', out)
elif cmd == 'focus':
    # near-miss conversion targets: AC puzzles whose AC or Stable-AC path is 0..3 above the record,
    # ties only where <= 2 teams hold it; best-first: few holders, small gap
    out = sys.argv[2]; P = problems(); score = {}
    for db in ('best_ac.json', 'best_sac.json'):
        for i, v in load(db).items():
            R, k = record(i)
            if not R: continue
            g = v['length'] - R
            if 0 <= g <= int(os.environ.get('FOCUS_GMAX', 6)) and (g >= 1 or k <= 3):
                a = 'ac-' + i.split('-')[1]
                pr = (0 if k <= 2 else 1, g)
                score[a] = min(score.get(a, (9, 9)), pr)
    ids = sorted(score, key=lambda a: score[a])
    open(out, 'w').writelines(P[a] for a in ids)
    print(len(ids), 'focus AC puzzles ->', out, '| few-holder first:', sum(1 for a in ids if score[a][0] == 0))
elif cmd == 'unsolved':
    out = sys.argv[2]; P = problems(); have = set(load('best_ac.json')) | set(load('best_sac.json'))
    ids = [i for i in P if record(i)[0] and i not in have]
    open(out, 'w').writelines(P[i] for i in ids); print(len(ids), '->', out)
elif cmd == 'plan':
    outdir = sys.argv[2] if len(sys.argv) > 2 else 'submit'; os.makedirs(outdir, exist_ok=True)
    rows = []
    for db in ('best_ac.json', 'best_sac.json'):
        for i, v in load(db).items():
            R, k = record(i); L = v['length']
            sub = submitted.get(i)
            if sub is not None and sub <= L: continue          # already submitted something at least as short
            if R is None: gain = 1.0                              # nobody has solved it
            elif L < R: gain = 1.0                                # sole holder
            elif L == R: gain = 2.0 ** (1 - (k + 1))              # join the tie (assumes we are not already one of the k)
            else: continue
            if gain <= 0: continue                               # rule: every submitted line must raise our score
            rows.append((-gain, i, v['moves']))
    rows.sort(); seen = set(); rows = [r for r in rows if not (r[1] in seen or seen.add(r[1]))]   # one line per id
    files = [rows[k:k + 500] for k in range(0, len(rows), 500)]
    for n, chunk in enumerate(files):
        print(f'  submit_{n:02d}.txt: {len(chunk)} lines, expected +{-sum(g for g, _, _ in chunk):.3f} points')
        with open(f'{outdir}/submit_{n:02d}.txt', 'w') as f:
            f.write('# acsolver: cost-bucketed macro beam + exact cap-16 ball + shortcut post-optimisation\n')
            for g, i, mv in chunk: f.write(f'{i}: {json.dumps(mv)}\n')
    beats = sum(1 for g, _, _ in rows if g == -1.0)
    print(f'{len(rows)} scoring paths ({beats} beats/unsolved, {len(rows) - beats} ties), expected +{-sum(g for g, _, _ in rows):.1f} points, {len(files)} files in {outdir}/')
