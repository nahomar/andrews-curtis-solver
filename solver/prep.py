"""Official manifest -> solver problem files (problems_ac.txt, problems_sac.txt, and the 424 training ones)."""
import json, os
OFF = os.path.expanduser('~/acc/official/competition')
m = json.load(open(f'{OFF}/tools/verifier/data/manifest.json'))
out = {'ac': open('problems_ac.txt', 'w'), 'sac': open('problems_sac.txt', 'w')}
for c in m['challenges']:
    r0, r1 = c['initial_relators']
    out[c['challenge_id'].split('-')[0]].write(f"{c['challenge_id']} {' '.join(map(str, r0))} | {' '.join(map(str, r1))}\n")
for f in out.values(): f.close()
t = json.load(open(f'{OFF}/examples/training_manifest.json'))
with open('problems_train.txt', 'w') as f:
    for c in t['challenges']:
        if c['move_spec_version'].startswith('ac-'):
            r0, r1 = c['initial_relators']; f.write(f"{c['challenge_id']} {' '.join(map(str, r0))} | {' '.join(map(str, r1))}\n")
print({k: sum(1 for _ in open(f'problems_{k}.txt')) for k in ('ac', 'sac', 'train')})
