"""Table of every run's held-out metrics (runs/*/eval.json)."""
import glob, json, os
KEYS = ['path_mae', 'path_partial', 'step_grow_worse', 'step_grow_dscore', 'step_consistency_mae',
        'sib_top1', 'sib_rule', 'sib_top1_grow', 'ball_opt_grow_improving', 'ball_fav_optimal', 'ball_rule_optimal', 'ball_fav_shortest']
runs = sorted(glob.glob('runs/*/eval.json'), key=os.path.getmtime)
print('run'.ljust(12) + ''.join(k.replace('ball_', 'b_').replace('step_', 's_')[:13].rjust(14) for k in KEYS))
for f in runs:
    r = json.load(open(f))
    print(f.split('/')[1].ljust(12) + ''.join(f'{r.get(k, float("nan")):14.3f}' for k in KEYS))
