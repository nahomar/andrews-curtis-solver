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
