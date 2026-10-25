## 2026-10-24 single-holder campaign: verified scoring tie, no record beat
- Prioritized puzzles with exactly one current record holder. Added `record_targets.py` to generate bounded, repeatable target lists from live snapshots and verified databases.
- Added 4,367 historical routes for 41 priority puzzles. These supplied useful intermediate states that shortest-only path filtering had discarded.
- Added `orbit_expand.cpp` for bounded neighborhoods around incumbent paths, followed by global path connections. Radius-4 exploration found `sac-05935` in 45 moves, improving our 46-move path and tying its sole holder.
- SAIR accepted all 8 scoring ties in submissions 4634 (5/5) and 4635 (3/3). One was the single-holder tie; the other seven were incidental ties with more holders. No global record was beaten.
- Radius-5 runs used four- and five-million-new-state budgets; both stopped during the last layer. They do not prove local or global optimality. `sac-05598` improved from 49 to 41 moves, one above its 40-move single-holder record.
- Added `nielsen_transfer.cpp` and `nielsen_weighted.cpp`. Weighted transport charges every conjugation letter and endpoint reduction; it found additional shorter paths, but no additional scoring record in this trial.
- Added an optional beam tail oracle (`orbit_search.cpp`). Initial pilot and wider waypoint runs did not earn points. Keep it experimental.
- Final databases: 950 AC and 945 Stable AC paths shorter than this campaign's starting databases, saving 24,366 moves in total. Every merge passed the official verifier; each board retains 5,929 solved paths.
- Validation: symmetry/BFS checks; inverse and move identities for 283 automorphisms; independent weighted raw-state Dijkstra comparison over 1,768 states, nine maps, both tracks; default-solver comparison on four small presentations.
- See `README_SINGLE_HOLDER.md`. Private path files and submissions remain local.

## 2026-10-24 symmetry-aware global path splicing: ADOPTED
- Recovered 892 previously solved puzzles missing from the active databases by merging a second local workspace through the official verifier.
- Added `orbit_splice.cpp` and `orbit_pool.py`: share paths across puzzle starts and 16 generator/relator symmetries, preserving the orientation and exact elementary move cost.
- Controlled one-move graph comparison: symmetry improves 4,104 AC / 4,232 Stable AC paths, worsens none, and reaches 7 extra starts per track.
- Added two-move bridge states and 7,583 alternative paths from past experiments. Two connection rounds: 1.86M graph nodes, 6.95M directed edges, 41.8 s, 2.13 GB peak resident memory.
- Self-test: symmetry identities and independently computed BFS distances on all 1,768 small-component states, both tracks, with/without symmetry. All real improvements were also checked by the official verifier.
- First batch: 390/390 solutions accepted by SAIR, submission 4631. The published leaderboard subsequently showed AC rank 16, score 0.9490; Stable AC rank 14, score 2.9161 (2026-10-24 17:25 UTC snapshot). All submitted candidates tied existing records; none beat a global record.
- Follow-up connection round: 41/41 further scoring ties accepted in submission 4632 (431 accepted solutions across both batches). Keep all solution files private. See `README_ORBIT.md` for usage and resource limits.
- Window polishing on the 12 highest-priority short Stable AC near misses (width 8000, radius 4) did not improve their paths. Continue prioritizing new routes and low-holder targets.

## 2026-09-23 mined macros (mine_macros.py)
- Mined 3-8 move n-grams from 310 near-record paths (<= record + 2), lift >= 3.
- Top patterns = repeated multiplies (lift up to 3.5e5): already covered by --power (868 of 880 steps in such runs shrink).
- 13 mixed patterns tested as fixed macros (--macros macros_mixed.txt), mid15, width 1500, seed 7:
  without 1202 moves / 13 solved vs with 1378 / 12 solved -> REJECTED (dilutes the beam).
## 2026-09-23 rejected: --minw (relator-imbalance bonus) fewer solved at 2 and 4.
## 2026-09-23 cap-17 ball (513,635,176 states, 9.7 GB): ADOPTED on the Mac
mid15, width 1500, seed 7: 1596 -> 1579 moves on 13 commonly solved, never worse, 6/13 shorter by 1-7.
Cloud trial VMs (16 GB / 8 GB RAM) stay on ball16.
## 2026-09-23 rebeam (waypoint restarts at 20/35/50/65/80% of the best path + splice): ADOPTED
mid15 starting from the best DB paths, width 2000, 45 s per waypoint: 1636 -> 1574 moves, 9/15 shorter, none worse.
## 2026-09-23 learned linear heuristic (--lin, fit_lin.py): REJECTED
Fit on near-record path states: val Spearman 0.709 vs 0.677 for the current min(cyc, best_child) score.
In search (mid15, w1500, seed 7): 1368 -> 2044 moves on 12 common; a few better (04643 80->73), several blow-ups
(01549 204->474). Distribution shift: fitted on on-path states, applied to everything the beam generates.
## 2026-09-23 radius 5/6 shortcut pass on top near misses: NO GAIN
10 ties at r5 (2-6 s each) and 6 gap-1 at r6 (10-23 s each): zero improvements. Near-miss paths are locally
optimal for 10-12 move windows; the record routes differ globally -> conversion needs route diversity (seeds/settings), not polishing.
## 2026-09-23 score noise for route diversity (--noise): NO GAIN at small budget
20 gap>=1 deep targets, width 3000, 60 s, noise 8 and 24: 0 shorter than best; splicing best+noisy: 0 improvements.
Our best paths come from far larger efforts (w12000 + rebeam + splice); cheap noisy runs do not add usable segments. Option kept, off by default.
## 2026-09-23 ultra-wide (width 60000, --bucket-mult 2) on 40 gap-1 few-holder targets: NO GAIN
0 beats, 0 paths shorter than our best (3 ties already held). Width is saturated at this level.
## 2026-09-23 cross-puzzle transfer (transfer.py): 127 AC + 127 SAC paths shorter than ours (all verified), none at record.
## Throughput note: normal-width focus reruns now convert ~1 per 100 targets; cloud static lists were re-solving held ties (fixed).
## 2026-09-23 GS class-distance oracle (--gs gs22.bin: 21.2M classes to length 22 from the proof agent's acenum)
Strong weights (gsw 8, pen 64): 1444 -> 2299 but solves 2 previously unsolved. Gentle (gsw 2, pen 16): 1444 -> 1518 (+5%),
better on 6/12 (09640 71->59), worse on 2. ADOPTED as 2 of 8 rotating focus settings (route diversity for splicing),
and on acc2 for never-solved puzzles (record < 100).
## 2026-09-24 expert iteration round 0 (learned guide from search-labelled sibling states)
Labels: 60k neighbour states of 1454 good paths, labelled by 4 s width-300 searches (acc1). Guide (ValueNet, init from vbias/all):
picks the best sibling 30-35% vs 37-39% for shortest-first (labels too noisy / length-biased). Replace mode in search: 7/15 solved (slow).
Blend mode (--guidew 2): 1202 -> 1294 on 11 common, but 09640 71->54, 01179 152->131, 04643 80->74 -> ADOPTED as 1 of 9 focus settings (diversity).
Next: exact labels (ball17) + deeper-search labels to remove noise/bias.
## 2026-09-24 expert iteration round 1 (deep labels)
16,023 neighbour states of 1,488 good paths; 2 label passes (width 1000, 15 s, radius 3, different seeds/settings), min taken.
Guide (init vbias/all): best-sibling pick peaks 0.398 (step 1000) vs shortest-first 0.412, then overfits (919 train groups).
Labels are now clean enough; the bottleneck is data volume (~1k groups). Beating the length rule likely needs ~100x more
labelled groups (millions of states), i.e. far more compute than the free trial provides. Parked.
## 2026-09-24 expert iteration round 2 (all deep labels, 3x data): NEGATIVE -> RL guide parked
2,935 train groups (lab2 + lab3a/b): best-sibling 0.29-0.32 vs shortest-first 0.371. More data did not help;
the guide cannot out-rank the length rule at this scale/architecture. Compute returns to the focus pipeline.
## 2026-09-24 RL variant: pretrain on 400k exact GS class distances, fine-tune on deep sibling labels: NEGATIVE
Pretraining fits well (held-out Spearman 0.75, MAE 1.5 macro steps) but sibling choice after fine-tune 0.29-0.32 vs 0.371.
Knowledge of length<=22 classes does not transfer to the 30-60 letter states where the beam chooses.
## 2026-09-24 gsw22.bin: weighted (move-cost) class distances via Dijkstra over all 21.2M classes (gs_wdist.cpp)
## 2026-09-24 weighted class-distance map (--gs gsw22.bin): mixed, adopted for diversity
mid15 w1500 seed 7: gsw 4 -> 1691 vs 1596 (+6%), better on 5 (09640 71->59), worse on 4; gsw 8 -> 1756 but solves 08786 (never solved).
Replaces one unweighted GS setting in the focus rotation (takes effect on the next loop restart).
## 2026-09-25 very wide single search (width 100000, bucket-mult 2, 1 h cap) on 20 top targets (<=2 holders, 1 off): NEGATIVE
0 beats, 0 shorter than our best; single wide beams are often worse than our best (which come from diverse runs + rebeam + splice).
Width is fully saturated. Focus loop back on 12 threads.
## 2026-09-25 AlphaZero-style self-play prototype (~/acc/selfplay/az.py): WORKS, learning
Imitation bootstrap (move-acc 0.34) + PUCT MCTS self-play with curriculum from our near-record paths:
k=4,6 solved 100%; k=8: 69% -> 75% -> 94% over 3 iterations; k=10: 12% at first try. Paths equal to ours (+0.3..+1.0), none shorter yet.
Long run started (500 iterations, 32 games, 48 sims) to measure how far the curriculum climbs on the Mac.
## 2026-09-26 whole-problem bidirectional similarity search (acs bidir): NEGATIVE
mid15, width 20k and 100k: frontiers never meet over 50-100 moves (exact-state meeting in a huge space); 0/15 improved.
Bidirectional search only works at window scale (20-40 moves), which `acs window` already exploits.
## 2026-09-26 self-play prototype long run: stalls at k=12 after 311 iterations (47-62% solved), never shorter than our paths.
## 2026-09-26 long windows (--wsizes 80,60, width 8000) on long15: 4042 -> 4019 (2/15 improved). Diminishing; 20-40 is the sweet spot.

## 2026-09-26 Outlier diagnostic (8 worst ratio puzzles, rec<=60, ours >=2x)
- Current base config (w8000, 180s) re-solves them at ~half our stored length (e.g. 297->146, 228->53): stored paths were stale from early weak runs.
- Still ~2.7x the record after refresh, so no points by itself; the true gap persists. maxlen 100 left most unsolved in 180s (search dilution).
- 2069 AC puzzles are >=2x record (results/stale2x.txt) — refresh candidates for idle compute, low priority vs near-misses.
- Result table: len100 / nojunk / cost39 / all relaxations -> no gain (mostly worse or unsolved). Limits are NOT the cause. Negative result.
- Reference: a public solver reaches 56 on ac-04334 where our from-scratch beam gets 140 -> gap is in search quality, not limits.
- Rebeam(w4000,60s)+window(w8000) on refreshed outliers: only 1-8% more (e.g. 140->133, 146->122; ac-09344 53->38, rec 33). Local polishing cannot close a 2.5x global gap; needs a different route, not a shorter version of the same route.

## 2026-09-26 Lineage cap (--lincap N --linper K): diverse beam, at most N slots per lineage per level, new lineages every K levels
- Outliers8, w8000 180s vs base: average no better, but finds different routes: ac-04011 154->82 (lc60p8), ac-06151 unsolved->156 (lc20p4), ac-08564 146->123 (lc200p8).
- Adopted as portfolio members in focus_loop SETTINGS (lc60p8, lc20p4). acs binary now includes lincap (default off = old behaviour; old binary acs.bak_prelin).
- 2026-09-27: stopped plateaued az.py long run (k=12 ceiling, 27h) — was draining battery while acs was paused.
- 2026-09-27 lost12 (records beaten by 1-2): exact shorten radius 5 and 6 -> 0 improvements. Our paths are locally optimal at radius 6; the new records use different routes.
- 2026-09-27 lost12 win-back: 6 rounds (w30000, 400s, incl. lincap, gsw) + rebeam w12000 + window w20000 -> 0/12 regained. Still 1-2 over record; left in focus/cloud rotation.

## 2026-10-24 "beat the long single-holder records" pilots: NEGATIVE
- Hypothesis: 602 k=1 records of 151+ moves and 718 of 81-150 are sloppy and beatable by our polishing tools.
- Polish pilot (60 closest few-holder records >=81 where we have a path; window 40/30/20 w4000 + rebeam w4000 60 s):
  10/60 improved, 16 moves saved, 0 at/below record; closest now 5 behind (ac-03595 115 vs 101). The single holders'
  long routes are ~10% better than ours and locally optimal for our tools -> not sloppy.
- Solve pilot (24 never-solved-by-us few-holder records 81-150, smallest relators; --maxlen 120 --maxcost 39 w4000 420 s lincap):
  0/24 solved. maxlen was not the blocker. From-scratch beam does not reach these.
- Only accidental new solves today came from symmetry splicing (4 puzzles). Few-holder never-solved puzzles (919) remain the
  cheapest "beat" (any solution at/below record = 0.5-1 pt) but need corpus/symmetry methods, not beam search.
- Corpus connection from bare starts (orbit_expand_starts = orbit_expand + --seed-starts): 20 never-solved k<=2 puzzles,
  radius 4 (270k states, 11 s, 0.3 GB) and radius 5 (2.15M states, 35 s, 1.2 GB): 0 connections to the 316k-state corpus.
  These starts are >= 6 moves (under symmetry) from anything we know. Radius 6 would be ~10 GB; not justified by the trend.
- Radius-4 sweep over all 791 never-solved k<=2 puzzles (orbit_expand_starts, 20 batches of 40, ~5 min total):
  6 NEW SOLVES (ac-00498 178/rec 140, ac-04723 175/137, ac-06581 174/142, ac-01415 174/136, ac-02265 695/217, ac-00971 118/103).
  First from-nothing solves of few-holder puzzles by any of our methods. Polishing + radius-5 sweep follow.

## 2026-10-24 Stable-finish search (--stable + ball16s.bin): POSITIVE, never used before
- All our SAC paths were AC paths + [16,15]. ball16s (8 signed-permutation endpoints, suffix costs) existed since Sept but no loop used it.
- Pilot on 40 few-holder SAC near misses (gap 1-4): solve w6000 150 s + rebeam w6000 60 s + shorten r5, all --stable:
  3/40 shorter; sac-09758 41->39 = TIES the record (few-holder). sac-03309 67->65 (rec 63), sac-07225 67->66 (rec 63).
- Rolling out: shorten r5 + rebeam over every SAC puzzle with gap 1-4 (results/stab2_*). Next: make it a loop stage + cloud.
- Radius-5 bare-start corpus sweep (786 never-solved k<=2): +2 solves (ac-02102 137/80, ac-04779 181/146). 8 new solves today.

## 2026-10-24 sacs.cpp — search in the TRUE Stable AC space (sac-r8-v1, 257 moves, rank 2..8, target = empty)
- New standalone solver: generic mixed-rank states, frozen move table rebuilt exactly as verifier/stable_core.py, forward moves
  AND exact predecessors (incl. un-stabilize / un-destabilize) for bidirectional search.
- Validation: predecessors() recovers 47,394/47,394 (state, move) pairs; all 5,944 DB Stable AC paths replay to the empty
  presentation; 0/400 mismatches vs the official Python core on random rank-3/4 sequences (25 moves each).
- `sacs window`: exact meet-in-the-middle shortcut between path states through rank<=3 states (rf/rb radii, cap 48).
  Sanity: 5 paths padded with a rank-3 detour [14,29,30,17] and [6,7] -> all 5 recovered exactly (-6). ~0.4 s per pass at rf3/rb3.
- new6 polish (2x window w8000 + rebeam w8000 120 s + shorten r5): 178->172, 175->169, 174->172, 174->168, 695->682, 118->114; still 11-32 above the records (k<=2). Left to the loop/cloud/other agent.
- `sacs window` on all 552 SAC gap-1/2 puzzles, rank<=3: rf3/rb3 (1 s/20), rf4/rb4 (15 s/20, full run 387 s), rf5/rb4 on 20:
  0 paths shortened. Our SAC paths are locally optimal in the stable space for windows up to 8-9 moves. NEGATIVE.
  -> the leaders' SAC advantage is not short rank-3 detours; candidates: the 8-endpoint stable finish (now in use, positive)
  or globally different routes (needs a from-scratch stable beam: `sacs solve`, next).
- `sacs solve` (from-scratch stable beam, elementary 257 moves, score = total letters + rankpen*(rank-2), exact ball16s finish):
  20 single-holder SAC near misses, w3000, 120 s: rankpen 2 -> 2/20 solved (51 vs 42, 72 vs 53); rankpen 0 -> 0/20.
  Without the AC solver's macro moves/lookahead an elementary beam is not competitive. NEGATIVE as a solver (as expected).
- `sacs tail` (restart from the state K moves before the end; stable beam w20000 may finish in < K): sanity: reproduces the
  known tail at K=8 (6/6), 12 (6/6), 16 (4/6) when equal length is allowed. K=12 on 20 single-holder targets: 0 shorter.
  Definitive sweep K=14 over all 552 SAC gap-1/2 puzzles running (results/tail14.log).
- sacs window rank<=4, rf5/rb5, cap 56 on the 39 single-holder SAC near misses: 0 shortened (595 s). Local optimality holds through rank 4 too.
- `sacs tail` K=14, w20000, rank<=3, over ALL 552 SAC gap-1/2 puzzles: 0/552 tails shortened (537 s). FINAL on this question:
  rank-3/4 detours do not shorten our Stable AC paths anywhere we can test exactly. The leaders' SAC edge = the 8-endpoint
  stable finish (ball16s + --stable, adopted today, positive), not stabilization moves. Stop investing in a stable macro beam.
