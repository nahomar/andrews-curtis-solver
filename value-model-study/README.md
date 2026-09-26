# vbias: value functions that collapse onto the cheapest proxy

Research scaffold on the SAIR Andrews-Curtis challenge. The question: why does a value network
trained for long-horizon, sparse-reward search learn a softened *length* heuristic, and which training
signals fix it?

## The measured problem (from Team Catsitter's public notes, `acc-challenge/STATUS.md` section 3.7)

- On verified solution paths, 14% of steps *grow* the presentation. Every step still lowers the true
  remaining distance by 1, yet their model scores growth steps as worse 71% of the time (+6 on average).
- In the exact ball, certified-optimal growth moves are scored as improvements only 10% of the time.
  The model's favourite move is the shortest neighbour 89% of the time. It picks an optimal move 70% of
  the time, while the plain shortest-neighbour rule manages 73%.
- A likely cause is that 401 of the 424 certified organiser paths never lengthen the presentation, so
  path supervision rarely shows useful growth.

Untried fixes, one flag each in `train.py`:

| flag | idea |
| :-- | :-- |
| `--w-step` | step consistency: v(s_t) - v(s_t+1) = 1 along paths, whatever the length change |
| `--grow-w` | upweight states/transitions whose path step grows the presentation |
| `--w-rank-ball` | pairwise ranking of sibling children with exact distances (what the beam compares) |
| `--w-rank-path` | listwise ranking: the taken move should be the favourite among its siblings |
| `--residual` | predict the residual over a least-squares length baseline |

## Setup

    # data comes from the public repos cloned next to this one (~/acc/acc-challenge, ~/acc/AC-Solver)
    python3 -m venv --system-site-packages .venv          # reuses system torch (MPS)
    .venv/bin/python tests_moves.py                       # move semantics == their C++/Python, all 5728 paths replay to (x, y)
    .venv/bin/python build_paths.py                       # 972k path states, 10% of instances held out
    .venv/bin/python build_ball.py --cap 12               # exact ball, 1.6M states, 12 s (--cap 14: 14.4M, ~2 min)
    .venv/bin/python train.py --tag base                  # baseline = their recipe (Huber on path + ball states)
    ./ablate.sh                                           # each fix alone, then all together
    .venv/bin/python compare.py

## Metrics (`evaluate.py`, all on held-out data)

- `step_grow_worse`: fraction of real growth steps scored as worse (their 71%; lower is better)
- `b_opt_grow_improving`: optimal growth moves in the ball scored as improving (their 10%; higher is better)
- `b_fav_optimal` vs `b_rule_optimal`: does the model beat the shortest-neighbour rule at picking optimal moves? (their 70% vs 73%)
- `sib_top1` vs `sib_rule`: on held-out paths, is the taken move the favourite?
- `path_partial`: Spearman with remaining moves after controlling for length (information beyond length)

## Caveats

- Path labels are moves remaining on a verified path, an upper bound on the true distance. The ranking
  loss on paths treats the taken move as best, which is noisy when siblings are also optimal.
- Ball distances are exact only within the length-capped graph, the same convention as their ball16/18.
  The cap-12 ball is smaller than their cap-18 one. For numbers comparable to theirs, build the C++ ball.
- The ball split is by state, so a held-out state's neighbours are usually training states. Path metrics
  are split by instance and are the clean ones.
- End to end: `beam.py` runs a raw-move beam from held-out starts (length scorer vs models). It is much weaker than their macro beam (length solves 3/20 short instances at width 1024), so read it as a relative comparison.

## Rules

Use only public repos and the official challenge API/data. Do not use the unauthenticated snapshot
endpoints or the recovered organiser materials mentioned in that repo's research notes.

## Results (held out, 6000 steps each, 2026-09-23)

| run | growth steps "worse" | optimal growth "improving" | picks optimal (rule 73.3%) | taken move is favourite (rule 34.0%) |
| :-- | :-: | :-: | :-: | :-: |
| base | 69.5% | 6.5% | 75.2% | 21.4% |
| step | 63.9% | 4.5% | 77.1% | 20.0% |
| groww | 69.0% | 2.0% | 74.8% | 20.5% |
| rankball | 68.9% | 0.5% | 77.2% | 20.3% |
| rankpath | 93.8%* | 0.0%* | 76.3% | 33.9% |
| residual | 70.2% | 4.5% | 75.7% | 19.2% |
| all | 79.9%* | 0.5%* | 78.4% | 26.6% |

\* ranking losses fix only the order of siblings, not the scale, so score deltas along a path are not meaningful for them.

Finding: the baseline reproduces Catsitter's bias (69.5% vs their 71%). No single fix removes it; step
consistency is the only one that reduces it (-5.6 pts). Combining everything gives the best move choice
(78.4% vs 73.3% for the shortest-neighbour rule), a real but small edge.
