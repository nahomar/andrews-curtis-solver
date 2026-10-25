# Symmetry-aware cross-puzzle path splicing

`orbit_splice.cpp` reuses saved solutions across different puzzles. It builds a
graph from all intermediate states on the supplied paths and finds shorter walks
through that graph. It also finds previously unrecorded one-move connections and,
optionally, two-move connections through new intermediate states.

The 16 symmetries are signed generator permutations and relator order swaps.
Canonicalization makes the graph smaller, while a separate orientation state
tracks how every elementary move must be mapped back. Generator renamings and
relator swaps are **not free moves**. AC paths end at exactly `(x, y)`. Stable AC
paths finish at a signed permutation of the basis, invert negative relators, and
destabilize. This is a path optimizer, not a proof of the conjecture or a globally
optimal search.

## Run

From the solver directory:

```sh
clang++ -O3 -std=c++17 -pthread orbit_splice.cpp -o orbit_splice
./orbit_splice --self-test
python3 orbit_pool.py --db best_ac.json best_sac.json \
  --alternatives results/*_ac.jsonl results/*_sac.jsonl --out pool.jsonl
./orbit_splice --problems problems_ac.txt --paths pool.jsonl \
  --two-move-cap 64 --join-rounds 2 --out results/orbit
python3 check.py results/orbit_ac.jsonl --db best_ac.json
python3 check.py results/orbit_sac.jsonl --db best_sac.json
```

Supply additional saved databases to `--db` to recover results from other solver
workspaces. `orbit_pool.py` retains every distinct database path and at most four
short alternatives per challenge by default. Alternative paths must be within
20 moves of the shortest database path; change `--max-extra` and `--slack` to
adjust this tradeoff. Only use your own paths or paths you are authorized to use.

The optimizer does not submit anything. Run the official verifier on the final
submission batch and refresh official records before selecting scoring paths.
Keep paths, databases, and competition submissions out of the public repository.

## Options and limits

- `--no-symmetry`: ordinary exact-state graph, useful for comparison.
- `--no-joins`: only use the supplied path edges.
- `--two-move-cap N`: allow new bridge states with total relator length at most
  `N`; default 0 disables two-move bridges.
- `--join-rounds N`: repeat connection building, including the bridge states from
  the previous round; default 1. More rounds can substantially increase memory.

The core reuses `acs.cpp`, with its 160-letter state limit. It skips input paths
that its internal verifier cannot replay, including unsupported higher-rank
Stable AC trajectories. Existing best-path databases retain those solutions;
merge results by taking the shortest officially verified path per challenge.
Outputs contain both tracks and are replayed internally before being written.

The self-test checks all symmetry/move identities, inverse mappings, endpoint
handling, and lifted shortest-path distances against an independent ordinary BFS
on all 1,768 states in a small length-bounded component, with and without symmetry
and for both tracks. Official verification is still required for real candidates.

## October 24, 2026 experiment

The initial corpus contained 21,466 distinct saved AC/Stable AC paths from two
local solver workspaces. Against ordinary one-move path splicing on the same
corpus, symmetry-aware one-move splicing shortened 4,104 AC and 4,232 Stable AC
paths, worsened none, and reached seven additional puzzle starts per track.

An expanded corpus added 7,583 short alternatives from earlier experiments.
Two rounds of connections at a 64-letter bridge cap produced 1,863,684 canonical
states and 6,945,897 directed edges. This run took 41.8 seconds and about 2.13 GB
maximum resident memory on the original machine. Every candidate merged into
the saved databases passed the official verifier.

This helped recover scoring ties, but did not beat a global record in this batch.
The next useful search targets are near misses with few record holders; simply
lowering a path that remains above the record earns no leaderboard points.
