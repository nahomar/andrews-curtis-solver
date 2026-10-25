# Continuing single-holder record campaign

The campaign prioritizes Stable AC `sac-05598` below 40 moves, followed by other
single-holder records. A shorter internal path is not a leaderboard gain unless
it beats the current record. Refresh records before every submission and use the
official verifier; only a successful server receipt confirms acceptance.

## Stable finishes from an ordinary AC table

`stable_ball_search.cpp` reuses an ordinary AC ball for all eight signed basis
endpoints. It applies signed generator permutations to a query, follows an AC
finish, transports every move back, and pays all required inversions and two
destabilizations. This makes the existing cap-17 AC table useful for Stable AC
without building a second nine-gigabyte table.

The resulting distances are exact within the saved table's length bound, not
unrestricted global distances. The beam uses them only to construct valid
solutions. It can still explore paths outside that table. Its seed-path oracle
also supplies the current incumbent as an initial upper bound.

```sh
clang++ -O3 -std=c++17 -pthread stable_ball_search.cpp -o stable_ball_search
./stable_ball_search --self-test
./stable_ball_search --targets TARGETS_sac.txt --paths INCUMBENTS_sac.jsonl \
  --ac-ball ball17.bin --out results/ball17_sac.jsonl \
  --radius 4 --cap 32 --budget 2000000 --width 6000 \
  --time-limit 30 --waypoints 3 --nielsen-depth 2 --stride 2 --polish-slack 1
python3 check.py results/ball17_sac.jsonl --db best_sac.json
```

The radius and state budget bound neighborhood exploration. The time limit
applies separately to each beam search, including waypoint restarts; it is not
an overall command timeout. The table itself occupies roughly 9 GB uncompressed.
Run one heavy search at a time. Input and output paths must be different.

Optional Nielsen transport tests short changes of generators and fully charges
both transported conjugations and the final basis reduction. `--polish-slack`
allows slightly longer candidate routes to be polished before rejecting them.
Optional `--macros FILE` reads sequences of 2–48 elementary moves, all fully
charged. The driver also exposes `--class-dedup`, `--reps`, and `--lincap` for
controlled diversity experiments.

The self-test builds independent AC and Stable AC tables at cap 6, then compares
all 1,768 raw states and replays every reconstructed Stable AC path.

## Alternative routes through the saved corpus

`orbit_variants.cpp` samples different shortest and near-shortest paths from the
orientation-aware graph, then applies local shortcut searches. It retains every
distinct polished route, which can also seed a later `orbit_expand` run.

```sh
clang++ -O3 -std=c++17 -pthread orbit_variants.cpp -o orbit_variants
./orbit_variants --self-test
./orbit_variants --problems problems_ac.txt --paths POOL.jsonl \
  --targets TARGETS_sac.txt --stable --ball ball16s.bin \
  --samples 500 --slack 4 --radius 3 --seed 241134 \
  --out results/variants_sac.jsonl
python3 check.py results/variants_sac.jsonl --db best_sac.json
```

`--slack` bounds how many moves a sampled route may exceed the graph's shortest
route. `--samples` counts distinct raw paths; sampling stops after at most twenty
times that many attempts if too few distinct paths are available. No generator
renaming or relator swap is treated as a free move. The self-test checks 2,373
sampled paths across orientations and both tracks, including the length bound.

## October 24 negative results to avoid repeating

- `sac-05598` stayed at 41 after radius-6 shortening, larger-table endpoint
  search, 283 Nielsen maps with polishing, wider beam searches, and bidirectional
  windows of 39/35/30/25/20 moves at width 30,000.
- Sampling 500 routes produced 379 distinct polished paths: 315 of length 41,
  57 of length 43, and 7 of length 45. None beat the 40-move live record.
- Exploring radius 4 around their combined states reached 555,348 states
  (552,768 newly added to the corpus graph), without a budget truncation. It
  produced no new best path.
- The larger-table/Nielsen/beam method also did not improve the first ten
  single-holder targets following 05598: 01240, 07264, 08961, 00428, 09009,
  00678, 04490, 09734, 00447, and 08012.

These are bounded negative experiments, not proofs that the records cannot be
beaten. Save the precise settings and results in the private campaign checkpoint
and try a materially different method before repeating these runs. Keep private
paths, checkpoints, snapshots, and submissions out of the public repository.

- Additional 05598 trial: 20 fully charged left-product/relator-conjugation
  macros, beam width 16,000, no class deduplication, and four waypoint restarts
  also left the path at 41 (77.2 seconds, 1,547,099 expanded states).


## Temporary-generator bridge search

`rank3_bridge.cpp` adds a fresh third generator at verified rank-two route
states, explores a bounded neighborhood with all 33 reversible rank-three
moves, and reconstructs shortcuts when neighborhoods meet. It charges both
rank changes and every intermediate move. With `--ball`, it also tries every
eligible generator removal and an exact cap-16 Stable AC finish. It retains
exact states; hashes are only used as indexes and cannot conflate states.

```sh
python3 prepare_rank3_seeds.py --official-tools OFFICIAL/competition/tools \
  --problems problems_ac.txt --paths VERIFIED_PATHS.jsonl --id sac-05598 \
  --cap 48 --out PRIVATE_SEEDS
clang++ -O3 -std=c++17 -pthread rank3_bridge.cpp -o rank3_bridge
./rank3_bridge PRIVATE_SEEDS sac-05598 5 28 8000000 41 CANDIDATES.jsonl \
  --ball ball16s.bin
python3 check.py CANDIDATES.jsonl --db best_sac.json
```

The positional numbers are radius, total relator-length cap, node budget, and
current incumbent length. `rank3_moves.inc` is generated from the official
`sac-r8-v1` table. Candidate output still requires full official verification;
the helper never submits. The seed preparer replays every input route through
the official move implementation before writing it.

Validation compared 19,661 reversible transitions and 2,160 destabilization
cases with the official implementation. A reconstructed bridge was also
accepted by the full official challenge verifier. A budget-truncated search is
a bounded negative experiment, not an optimality proof. The nearest-seed
partition can miss useful alternative prefixes, even without truncation.

The first 05598 experiments used baseline seeds at radius 4/cap 40/3 million
nodes and 379 alternate routes at radius 4/cap 36/6 million nodes. Both reached
the node budget and left the incumbent at 41. The first ten following
single-holder targets also remained unchanged after 500 sampled routes each.


The extended 05598 search (379 route variants, radius 5, cap 28, 8 million
nodes, arbitrary eligible generator removal followed by a stable cap-16
finish) also stayed at 41. It expanded 6,048,915 states in 42.8 seconds and
hit its node budget. Five further near misses, 08599, 03714, 02264, 05105,
and 09204, remained unchanged with the larger-table/Nielsen/beam method.

A refreshed inventory found 683 single-holder challenges absent from the
verified local database. Absence does not mean they have never been attempted:
for example, 01613 already has an unsuccessful older 120-second AC beam run.
Inspect historical settings before selecting a new experiment for these.


## Discovery and path deformation

`stable_discover.cpp` uses the same charged cap-17 endpoint oracle without an
incumbent. It supports bounded searches of unsolved challenges with `--targets`,
`--ac-ball`, `--out`, `--width`, `--time-limit`, `--max-depth`, `--maxlen`,
`--class-dedup`, `--reps`, and `--seed`. Its small-table self-test checks all
1,768 exact states; a separate discovery smoke path passed official replay.

`path_deform.cpp` samples equal-cost and slightly longer local replacements
using two exact-state neighborhoods, accepting occasional temporary detours
and periodically applying strict shortening. It verifies every emitted full
route. Its flags include `--problems`, `--paths`, `--out`, `--radius`, `--slack`,
`--budget` (per neighborhood), `--seconds` (per target), `--steps`, and `--seed`.
The first 2,000-step radius-4 experiment emitted 110 paths for 05598 and 177
for 00678. All passed official move replay; best lengths stayed at 41 and 42.

`record_targets.py --include-matches --submitted submitted.json` retains an
unsubmitted match to another team's record as a promising one-move target.
It excludes a record already held according to the accepted-submission ledger.
The original default gap filter remains unchanged without this option.

The ten-target route union added 2,391,179 states without exhausting its
3-million-node budget. The official verifier approved 92 AC and 91 Stable AC
improvements over the campaign checkpoint. Safe merging improved the active
databases by 89 AC and 87 Stable AC paths, saving 206 moves. It reached the
42-move single-holder record for 00678 but produced no strict record beat.
That matching path was not submitted under the strict-beat-only policy.


`nielsen_discover.cpp` directly tries bounded generator changes before table
lookup, reconstructs elementary moves in the original generators, and pays
for the complete final basis reduction. Unlike `nielsen_ball`, it does not
require an incumbent. Compile like the other helpers; flags are `--problems`,
`--ac-ball`, `--out`, and `--depth` (default 4). Its synthetic discovery path
passed official replay. A depth-4 scan of 683 single-holder challenges found
three new officially verified local solutions: 07005=262, 07874=70, 09996=96.
Their live records were 93, 55, and 62 respectively, so none was submitted.
The five bounded beam-discovery tests found no solution; do not repeat those
settings. A generator-map scan following bounded elementary prefixes is an
untested next approach. The current graph expansion combines these three new
proofs with the recent path-deformation and 00678 alternate routes.


## Combining elementary prefixes, waypoints, and generator changes

`nielsen_discover` now supports `--radius`, `--cap`, `--budget`, `--seconds`,
optional incumbent `--paths`, and `--waypoint-stride`. It queries charged
Nielsen finishes after bounded elementary prefixes. With waypoints enabled,
complete verified incumbent prefixes seed the neighborhood; all their costs
are retained. Exact state keys prevent accidental merging through hash
collisions. The prefix budget and time cap can truncate the experiment, and
the waypoint dominance rule is heuristic, so a negative run is not an
optimality proof. The default radius 0 retains direct-discovery behavior.

Validation includes a case unavailable with a zero-length prefix that becomes
a valid five-move solution after one prefix move, and a separate waypoint
reconstruction checked by official move replay. The priority-ten trial found
a new verified solution for 05314=90 and shortened 09996 to 93, but no record.
The first hundred additional unsolved single-holder challenges yielded a new
verified 01416=55 solution against a live record of 48. The three waypoint
trials left 05598=41, 00678=42, and 03714=53 unchanged; see private logs for
individual timeout and budget truncation flags.

`nielsen_beam.cpp` exposes the transformed finish lookup as an upper-bound tail
oracle throughout beam search. It never treats a transformed distance as an
admissible lower bound or counts generator changes as free moves. Its flags
include `--targets`, optional `--paths`, `--ac-ball`, `--out`, `--depth`,
`--width`, `--seconds`, and `--seed`. The discovery smoke path passed the
official move replay before campaign use.

## Bounded weighted generator transfers

`nielsen_weighted` now accepts `--stable-only`, `--map-start`, `--max-maps`, and `--seconds` for checkpointable work. The time limit covers the map loop and is checked between maps; graph preparation and the final active map can take additional time. Map indices are zero-based in `--map-start`, and the progress log prints completed indices one-based.

Optional `--candidate-slack N --polish-radius R` allows a transported route to be slightly longer than the incumbent before shortcutting. Transported length is an upper bound; cancellation across the prefix/tail join can reveal an improvement. Default zero values preserve the original behavior. Every resulting improvement is fully replayed internally and must also pass the official verifier before database merging or submission.

Validation: the independent weighted search comparison passed on 1,768 raw states for nine maps in both tracks. The bounded helper's cancellation smoke shortened a valid four-move route to two moves, and official Stable AC replay reached the empty presentation.

Weighted Dijkstra now stops beyond the largest tail length admitted by the
existing `prefix + tail < incumbent + candidate_slack` filter. Because prefix
costs are nonnegative, this skips only candidates that the same run would
already discard before polishing. This is an exact performance restriction
relative to that filter, not a new claim about unrestricted optimal paths.
Independent raw-state comparisons also cover distance limits 4, 8, and 12 for
all nine test maps in both tracks. The default uncapped constructor remains
available for those tests.

`nielsen_weighted --graph-cache FILE` can reuse the expensive two-round route
graph preparation. The local cache records content fingerprints for the exact
problem and route files, graph settings, and a format version. Loading checks
dimensions, edge fields, a full-file checksum and trailing bytes. Changed or
corrupt inputs are rebuilt; writes replace the cache atomically. Fingerprints
and checksums guard accidental mismatches and corruption, while candidate
paths still undergo full replay and official verification. Bump the cache
version if graph-construction or orientation semantics change.

Validation covers every key, orientation and edge in a round trip; mismatched
inputs/settings, corrupted payload and extra trailing data are rejected. The
weighted independent distance tests still pass. A separate cached-versus-fresh
integration test produced the same three-move solution from a five-move
incumbent, and the complete result passed official Stable AC replay.

## Bucket search diversity correction (24 October, 20:00 UTC)

The bucket beam previously ignored `--class-dedup 0` and still enforced the `--reps` cap on cyclic classes. The flag now disables that cap, retaining distinct raw states up to the ordinary beam width. Earlier experiments labeled “class dedup off” did not actually disable the cap and should not be treated as equivalent negative results for the corrected solver. Default enabled behavior is unchanged.

`bucket_dedup_test.cpp` compares an elementary-only search against an independent exact breadth-first traversal: 9,400 states were reached with class pruning disabled, independent of the reps setting; enabling reps=1 retained 187 states. The check passed. Active `acs`, `stable_ball_search`, `stable_discover`, and `nielsen_beam` were rebuilt with this correction. All candidate paths still require official verification.

## Longer stabilized detours

`rank3_beam SEEDS ID STABLE_BALL WIDTH CAP SECONDS BEST_LENGTH SEED OUT` searches deeper than the existing radius-limited rank-three bridge. Seed prefixes and suffixes must come from `prepare_rank3_seeds.py`, which fully replays each source route with the official implementation. The search charges each initial stabilization and every rank-three step, checks all three allowed destabilizations, and pays the exact stored suffix or stable table finish. It keeps distinct raw states by cost and ranks them using reduced word lengths. This bounded beam is heuristic; no improvement is not an optimality proof.

Validation: a synthetic route with 29 moves was shortened to 11 through three rank-three steps and a paid destabilization. Official Stable AC replay accepted the complete emitted path. Existing rank-three move and destabilization implementations retain their prior independent verifier comparisons. The active compiled helpers are installed via atomic replacement to avoid stale executable caching on macOS.

## Optional class-distance guidance

`stable_ball_search` now accepts `--gs FILE --gsw WEIGHT --gspen PENALTY`, exposing the existing solver's GS class-distance table as a ranking heuristic. The local `gs22.bin` contains 21,194,887 classes at cyclic length at most22. Distances in this quotient graph are only search guidance: all actual elementary moves and the Stable AC finish are still charged and replayed. The driver reports each initial table lookup. `--look` must be positive for the heuristic to rank candidates.

A tiny GS-guided search shortened a verified five-move route to three moves, and official replay reached the empty presentation. This integration does not change default search behavior without `--gs`.

## Incumbent growth profile

`stable_ball_search --profile-weight 16 --profile-ahead 4` adds a preference for cyclic lengths occurring around the same cost on a verified incumbent route. The look-ahead in the profile allows progress several moves ahead of that route. This can retain the temporary growth needed by difficult presentations: the current 05598 route grows from cyclic length19 to46 before collapsing. A profile is regenerated separately for each starting waypoint. Ordinary length lookahead is disabled in this mode because it would erase the growth preference; combining profile mode with GS guidance is rejected. All ordinary move costs and full-path checks remain in force. The profile smoke shortened a five-move route to three and passed official replay.

## Word-matching waypoint bridges

`edit_bridge` searches from both ends of an incumbent window. It ranks states
by the sum of the two word edit distances to the opposite endpoint, using
Myers' bit-vector algorithm. Unlike the older bigram window beam, it keeps
exact state keys and removes duplicate candidates before applying the width
limit. Every reconstructed bridge is replayed and its complete replacement
path must strictly shorten the verified incumbent. The heuristic only ranks
the beam; it never serves as a pruning lower bound.

Flags: `--problems`, `--paths`, `--out`, `--windows 39,35,30,25,20,15,10,8`,
`--width 8000`, `--cap 64`, `--stride 2`, `--seconds 120`, `--seed 1`.
The time budget applies separately to each challenge. The cap must be at most
64 so each nonempty relator fits the 63-letter bit-vector pattern. Full
official verification remains mandatory before merging improvements or
submitting candidates. The bounded beam does not prove optimality.

Time-limited runs print the interrupted window length and start position.
Resume with that window first in `--windows` and `--first-at POSITION`; the
offset applies only to the first eligible window. Subsequent window sizes and
any newly shortened route start from zero. The resume-offset smoke checked
that an intentionally skipped window is not searched and replayed its output
with the official verifier.

Validation: `--self-test` compared 10,000 random word pairs with an independent
dynamic-programming edit-distance implementation, then checked direct and
identical-endpoint bridges. The five-to-three-move integration smoke passed
official Stable AC replay. The initial 05598 experiment finished 71 windows
and 4,377,526 expanded states without improvement; settings are in the private
campaign checkpoint, so do not repeat that negative trial unchanged.
