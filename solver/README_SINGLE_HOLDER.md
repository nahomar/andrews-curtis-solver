# Search for records held by one team

Refresh the official snapshot, then select near misses with exactly one holder.
The selector excludes paths that already match or beat the record. It sorts by
the remaining move gap and limits the number of targets per track.

```sh
python3 sair.py snapshot
python3 record_targets.py --records records.json \
  --db best_ac.json best_sac.json --problems problems_ac.txt \
  --out-dir results/one-holder --limit 24 --max-gap 6
```

The output contains separate AC and Stable AC target lists, their incumbent
paths in `seeds.jsonl`, a combined AC-labelled start list, and `targets.json`
with lengths and holder counts. Refresh it after improving the databases.
Only a path at or below the live record can earn points. Shortening a path that
remains above the record is useful search progress, but is not a scoring gain.

## Explore alternative connections near the selected paths

`orbit_expand` explores a bounded neighborhood around every state of each seed
path, then connects those states to the existing corpus of verified paths.
It preserves orientation and charges every elementary move. The node budget
limits newly added neighborhood states, not the preexisting corpus or RAM.
The last BFS layer can be incomplete when this budget is reached; this is not
an exhaustive proof that a shorter solution does not exist.

```sh
clang++ -O3 -std=c++17 -pthread orbit_expand.cpp -o orbit_expand
python3 orbit_pool.py --db best_ac.json best_sac.json \
  --alternatives results/*_ac.jsonl results/*_sac.jsonl --out results/pool.jsonl
./orbit_expand --problems problems_ac.txt --paths results/pool.jsonl \
  --seed-paths results/one-holder/seeds.jsonl --radius 3 --cap 64 \
  --budget 2000000 --out results/one-holder/neighborhood
python3 check.py results/one-holder/neighborhood_ac.jsonl --db best_ac.json
python3 check.py results/one-holder/neighborhood_sac.jsonl --db best_sac.json
```

Large runs need substantial memory. Start with radius 3 and a small target list.
The October 24 radius-5 runs used budgets of four and five million new states;
both stopped partway through their final layer. Reusing a larger set of historical
routes was also useful: paths discarded by a shortest-only filter can contain
valuable intermediate states.

## Reuse paths through changes of generators

`nielsen_transfer` tests short Nielsen automorphisms against stored tails.
`nielsen_weighted` improves on this by finding the shortest tail according to
its actual cost after transport. Conjugation by a word costs one move for each
letter, and reducing the resulting basis to the official endpoint is fully
charged. No generator change is treated as a free competition move.

```sh
clang++ -O3 -std=c++17 -pthread nielsen_weighted.cpp -o nielsen_weighted
./nielsen_weighted --self-test --ac-ball ball16.bin --sac-ball ball16s.bin
./nielsen_weighted --problems problems_ac.txt --paths results/pool.jsonl \
  --targets results/one-holder/starts.txt \
  --incumbents results/one-holder/seeds.jsonl \
  --seed-paths results/one-holder/seeds.jsonl \
  --ac-ball ball16.bin --sac-ball ball16s.bin \
  --depth 1 --stride 2 --out results/one-holder/weighted
python3 check.py results/one-holder/weighted_ac.jsonl --db best_ac.json
python3 check.py results/one-holder/weighted_sac.jsonl --db best_sac.json
```

Depth 1 evaluates the identity and eight elementary generator changes. Each
map requires another shortest-path pass through the graph, so increase depth
cautiously. The self-test compares weighted distances against an independent
raw-state search on 1,768 states, for nine maps and both tracks. It also checks
inverse and move-transport identities for 283 automorphisms. Every real output
must still pass the official verifier before database merge or submission.

Both methods inherit the rank-two and 160-letter limits of `acs.cpp`. Valid
higher-rank Stable AC paths that cannot be replayed internally are skipped;
the verified best-path databases retain them.

## Experimental beam oracle

`orbit_search.cpp` lets the beam search finish through a stored valid tail.
The optional `TailOracle` defaults to null in the original solver, preserving
its normal behavior. Oracle distances are upper bounds used to construct
solutions, never admissible lower bounds used to prune search. The first
targeted runs did not produce a scoring improvement, so this remains an
experimental option rather than a replacement for the existing search loop.

The tools do not submit automatically. Refresh the official records, select
unsubmitted scoring improvements, verify the complete submission batch, and
check the server receipt. Keep private paths, database files, and submission
payloads out of the public repository.
