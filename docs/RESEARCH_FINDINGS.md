# Search and learning findings, November 2026

The campaign optimizes the number of **atomic Stable AC moves**, not merely
whether a presentation can eventually be trivialized. A shorter local path is
not a new competition record unless it beats the live record and the server
accepts the complete officially verified submission.

## Why the learning experiments did not establish an advantage

Earlier self-play and reward-shaping pilots did not establish reliable
generalization to strict record-beating budgets. Improved training accuracy,
next-move prediction, or finishing rate did not by itself show an advantage at
finding a shorter path under an equal search budget. These are observed gaps
between metrics, not a proof that reinforcement learning cannot help.

The subsequent objective was **goal-conditioned shortcut recovery**: take two
waypoints on a historical path and connect their exact states in fewer atomic
moves. This supplies a concrete goal and budget. Historical shortening pairs
were replayed using the official implementation; whole challenges and exact
state-goal pairs were excluded from fitting where held-out tests were claimed.
Failed attempts were retained, and development results were distinguished from
fresh pilot and confirmation results.

A late comparison on 30 consumed historical cases produced:

| Decoder/search | Shortcuts recovered | Total atomic moves saved |
| --- | ---: | ---: |
| Uniform bidirectional beam | 0/30 | 0 |
| Frozen neural beam, fixed opposite endpoint | 1/30 | 4 |
| Frozen neural beam, opposite frontier goals | 1/30 | 8 |
| Confirmed rewritten-seed search | 21/30 | 67 |
| Experimental query-index search | 22/30 | 70 |

The beam experiments shared a total one-second/5,000-candidate-edge allowance,
16 retained beam states (eight per side), and three distinct successors per
parent. Their reference-search results were reused from saved runs; the
reference engines use their own search structure and expansion accounting.
These are bounded local comparisons, not a universal ranking of algorithms.

The neural variants were not promoted. The query-index result also did not pass
its previously specified improvement gate. Stronger local move prediction had
not translated into useful multi-step search in these tests.

An initially suspected cause was that reverse predecessors lacking an alias in
the forward policy would dominate selection through neutral scores. An offline
audit of 1,000 saved reverse contexts did **not** support that account: 29.8% of
eligible predecessors lacked aliases, but only 12.1% of the top-three selections
did. That audit used a counterfactual remaining budget of eight and lacked the
original depth/seen filters and beam scores; it cannot establish the actual
rollout's causal failure. A true uniform control also recovered no shortcuts.
The evidence supports changing the method or objective, not repeating the same
reward-only training or claiming a proven diagnosis.

## What is released

`shortcut-search/` contains the confirmed unlearned search components: exact
rank-changing edges and reverse preimages, paid original-path boundary seeds,
paid conjugacy joins, universal move identities, compatible shortcut
composition, and optional native word kernels. The package has a documented
public API and synthetic tests. Formatting, imports, input validation, and the
native fallback were adapted from the research scripts; `source_provenance.json`
records the original source hashes.

These results were measured using private historical trajectories. They are
aggregate reports, **not independently reproducible benchmark scores from the
public package**. Private trajectories, model weights, and evaluation datasets
are intentionally omitted. The repository's earlier `selfplay/`,
`guide-learning/`, and `value-model-study/` folders contain the earlier learning
prototypes. The many later experimental training scripts are not bundled into
this standalone search library.

## Competition check

On November 5, a further 597 previously untried production waypoint queries
were tested with the confirmed engines, across nine single-holder challenges.
They produced no shortening and no new record submission. The unchanged
41-move incumbent for the primary target still exceeded its 40-move live
record; completed negative queries were skipped rather than restarted.

The next work is to test genuinely untried inputs and develop objectives that
improve connection success under a fixed atomic budget. Any learned replacement
needs a specified development gate, a fresh pilot, and independent confirmation
against confirmed baselines. A bounded negative result is not a proof that a
shorter path does not exist.
