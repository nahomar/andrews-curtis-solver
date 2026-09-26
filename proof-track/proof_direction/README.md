# Short balanced presentations: an exhaustive, certified classification up to total length 16 (rank 2, AC direction)

**Status:** computer-verified. Every AC-triviality claim comes with explicit official `ac-r2-v1` move sequences,
and those sequences are replayed by the organisers' own verifier code (`tools/verifier/core.py`). Every "not the trivial
group" claim comes with an explicit permutation representation, which the checker verifies. Completeness is checked
by an enumeration written separately from the search code (`check.py`, Python standard library only).
There is no Lean formalisation: no Lean toolchain is installed on this machine (see "Limitations").

## 1. Main result

Notation: `x X y Y` stand for x, x⁻¹, y, y⁻¹. The length of a presentation ⟨x,y | r₁, r₂⟩ is |r₁|+|r₂|, measured on
freely reduced words. This matches `AC.Relators 2` in `tools/lean/AC.lean`, where relators live in `FreeGroup (Fin 2)`.
A *signed letter permutation* is one of the 8 automorphisms of F(x,y) that permute {x, y} and invert either letter.

Six presentations of the trivial group, each of length ≤ 14:

| name | relators | where it comes from |
|---|---|---|
| AK3 | `xxxYYYY`, `xyxYXY` (x³=y⁴, xyx=yxy) | Akbulut–Kirby AK(3) |
| P1  | `XyyxYYY`, `YxxyXXX` (x⁻¹y²x=y³, y⁻¹x²y=x³) | Johnson (1980); the open "P1" of Carreras (2026) |
| P2  | `XyyxYYY`, `XyxxyXX` = MS(2, yx²yx⁻²) | Miller–Schupp; hard case of Shehper et al. (2024) |
| C3  | `xxyXy`, `xyyyyyxYY` (x²yx⁻¹y, xy⁵xy⁻²) | found here |
| C4  | `xxxxyXy`, `xyyyxYY` (x⁴yx⁻¹y, xy³xy⁻²) | found here |
| C6  | `xxyxxYY`, `xyXYYXy` (x²yx²y⁻², xyx⁻¹y⁻²x⁻¹y) | found here |

**Theorem A (length ≤ 14; computer-verified).** Let P = ⟨x,y | r₁, r₂⟩ be a balanced presentation of total length
≤ 14 that presents the trivial group. Then either P is AC-trivial, or P is AC-equivalent to φ(Q) for some signed letter
permutation φ and some Q ∈ {AK3, P1, P2, C3, C4, C6}.

*Corollary.* The Andrews–Curtis conjecture holds for every balanced 2-generator presentation of length ≤ 14 if and
only if it holds for the six presentations AK3, P1, P2, C3, C4, C6. (AC-triviality is invariant under φ, see Lemma 2.)
For AK3, the φ in Theorem A can be removed: Panteleev–Ushakov showed that every automorphism of F₂ is AC-realised on AK(n).

Finer detail. Up to rotating or inverting relators, swapping them, and signed letter permutations, there are exactly
4070 classes of length exactly 14 with trivial abelianisation. Of these, 4052 are certified AC-trivial and 4 have
certified nontrivial finite permutation quotients. The remaining 14 all present the trivial group, and each is certified
AC-equivalent (up to φ) to one of the six presentations above: AK3 5, C3 4, P2 2, P1 1, C4 1, C6 1.

At length 13 the same computation reproduces the Havas–Ramsay theorem. There are 2359 classes: 2352 are AC-trivial,
5 are nontrivial groups, and 2 are AC-equivalent (up to φ) to AK(3). At length ≤ 12 it reproduces Miasnikov–Myasnikov:
all trivial-group presentations are AC-trivial, and the single residual class at length 12 is a nontrivial group.

**Theorem B (lengths 15 and 16; computer-verified, weaker form).** Every presentation of the trivial group of length
≤ 16 is AC-trivial or lies in one of 287 explicitly certified finite "residual components". Each component consists
of presentations that are pairwise AC-equivalent up to φ. Per length:

| length | classes (det ±1) | certified AC-trivial | certified nontrivial group | residual (trivial group) | residual components meeting this length |
|---|---|---|---|---|---|
| ≤ 11 | 530 | 530 | 0 | 0 | 0 |
| 12 | 591 | 590 | 1 | 0 | 0 |
| 13 | 2 359 | 2 352 | 5 | 2 | 1 (AK3) |
| 14 | 4 070 | 4 052 | 4 | 14 | 6 (AK3, P1, P2, C3, C4, C6) |
| 15 | 17 021 | 16 767 | 22 | 232 | 76 (of which AK3 72 classes, C3 31, P2 12, P1 2, C6 2) |
| 16 | 29 929 | 29 304 | 35 | 590 | 265 (of which AK3 82, C3 27, P2 23, P1 8, C4 4, C6 3) |

Here a "class" is an orbit under rotation or inversion of either relator, swapping the relators, and the 8 signed
letter permutations. Full lists are in `data/residual_classes16.txt` (class → component) and
`data/residual_components16_c28.txt`.

### What is proven, what is computer-verified, what is conjectural

* **Proven by hand (Section 3):** the reduction lemmas. Up to AC moves and φ, it suffices to treat cyclically reduced
  pairs with det ±1. Each certificate line implies the stated AC-equivalence.
* **Computer-verified (with independently checkable certificates):** Theorems A and B. That is, every AC-trivial or
  AC-equivalence claim (68 688 certified edges), every nontriviality claim (67 permutation representations), and
  completeness of the enumeration up to length 16.
* **Computed but not certificate-checked:** the statement that the 14 length-14 residual classes (and the 822 residual
  classes of lengths 13–16) *do* present the trivial group. This comes from Todd–Coxeter coset enumeration (index of
  ⟨x⟩ = 1 together with trivial abelianisation). Two independent implementations agree on all 960 classes: our `grp.cpp`
  and SymPy 1.14 (`data/sympy_crosscheck16.txt`). This fact is not needed for Theorems A and B as stated. It only shows
  that the residuals are genuine open cases rather than presentations of nontrivial groups.
* **Not claimed / open:** we do **not** show that the six classes are pairwise AC-inequivalent, or that any of them is
  AC-nontrivial. We only know that their components in the length-capped graph are disjoint up to total length 28:

  | | AK3 | P1 | P2 | C3 | C4 | C6 |
  |---|---|---|---|---|---|---|
  | component size at cap 28 (classes) | 8 650 489 | 680 700 | 1 880 041 | 5 981 990 | 108 455 | 100 703 |

  C4 and C6 stay separate from the trivial component even at cap 30, with 623 460 and 5 307 099 classes. Two other
  length-14 residuals from the cap-24 stage were shown AC-trivial only at cap 26, via intermediate presentations of
  length 22–26. So "not trivialised within cap 28" is weak evidence at best.

## 2. Novelty relative to the literature (as far as we could verify)

* Miasnikov–Myasnikov [MM] showed that all balanced 2-generator presentations of the trivial group of total length
  ≤ 12 are AC-trivial. Havas–Ramsay [HR] showed that at length 13 every such presentation is AC-trivial or
  AC-equivalent to AK(3). We **reproduce** both, now with replayable certificates in the official move format.
* Length 14: Carreras [C26] (arXiv, July 2026) states that length 14 has "never been exhaustively classified". The
  hard length-14 cases in the literature are Shehper et al.'s [S24] six Miller–Schupp presentations and Johnson's P1.
  Of these, [S24] claimed four are AC-equivalent to AK(3), and [C26] certified two of those plus two further
  equivalences. Our computation, independently of those move sequences, puts these seven presentations into exactly
  three capped components: AK3, P1 and P2 (`data/lab_c28.txt`). This agrees with [C26]: P1 ~ MS(2, yx²y⁻¹x⁻²) and
  MS(2, yx²yx⁻²) ~ MS(2, x⁻²y⁻¹x²y⁻¹), with MS(3, yx²y) and MS(3, y⁻¹x²y⁻¹) in AK(3)'s class.
  **What is new:**
  1. Exhaustiveness at length 14: every length-14 trivial-group presentation falls into one of these components or
     into three further components, C3, C4 and C6.
  2. The same exhaustive reduction at lengths 15 and 16.
  3. The explicit presentations C3, C4, C6, which are not in the capped components of AK3, P1, P2 or the six MS
     presentations.

  To our knowledge none of these has been published. **Caveats:** we could not read the full text of Bowman–McCaul
  [BM] (paywalled), which extended the Havas–Ramsay search. Fagan et al. [F26] enumerated 213 946 balanced
  presentations of length ≤ 19 and solved 125 192 of them, without (as far as we could see) a per-length exhaustive
  statement. We have not compared C3, C4, C6 against their unsolved set. They may therefore already appear, unlabelled,
  in [BM] or in the AC-19 data.
* Stable AC: AK(3) is stably AC-trivial [S24; Lisitsa 2025]. We did not study stable moves for C3, C4, C6. They are
  natural next targets.

## 3. Method and proofs of the reduction lemmas

**Lemma 1 (AC-invariant normalisations).** Each of the following changes P only within its AC class:
* conjugating a relator by a letter, which includes rotating a cyclically reduced relator and cyclic reduction;
* inverting a relator;
* swapping the two relators. This is derivable, as noted in `AC.lean`: relator permutations are reachable.

**Lemma 2 (signed letter permutations).** Let φ be an automorphism of F₂. If R → S is an AC step, then so is
φR → φS: apply the same move type, with conjugator φ(w) in place of w. Hence R ~AC S implies φR ~AC φS. For a signed
letter permutation φ, φ(x, y) is (x^±1, y^±1) up to order, which reaches (x, y) by inversions and a swap. So R is
AC-trivial ⟺ φR is AC-trivial.

**Lemma 3 (what must be enumerated).** If P presents the trivial group, then its abelianisation ℤ²/⟨exponent vectors⟩
is trivial. So the 2×2 exponent-sum matrix has determinant ±1, and in particular neither relator is empty. By Lemma 1,
P is AC-equivalent to the pair of cyclic reductions of its relators, which has length ≤ |P|. So Theorem A for all P of
length ≤ 14 follows from the statement for cyclically reduced pairs of length ≤ 14 with det ±1, one representative per
orbit of the symmetries in Lemmas 1–2.

**Certificates.** A line `EDGE nu nv pu pv h swp m₁ … m_k` asserts the following. Replaying the official moves
m₁…m_k (ids 0–13 of `ac-r2-v1`) from the exact state (pu, pv) gives a state Q with (nu, nv) = h(Q), relators swapped
if swp = 1. Then (nu, nv) ~AC h(pu, pv) by Lemma 1. Connectivity in the undirected graph of verified edges therefore
gives AC-equivalence up to φ (Lemma 2), and connectivity to `ROOT x y` gives AC-triviality.

**Search graph ("GS moves", as in Havas–Ramsay style searches).** The nodes are orbit representatives. An edge
replaces u by the cyclic reduction of rot_i(u)·rot_j(v^±1), or symmetrically replaces v, provided the total length
stays ≤ cap. Every edge is realised by explicit official moves (rotations, one multiplication, cyclic reduction, final
rotations), and these moves are exactly what the certificates contain.

**Pipeline** (`reproduce.sh`):
1. `acenum bfs --cap 22`: breadth-first search of the component of (x,y) among orbits of total length ≤ 22. The
   result has 21 194 887 classes and was complete (9 min, 1 thread).
2. `acenum class --len 17`: enumerate all orbit representatives with det ±1 up to length 17, and flag membership.
3. `grp`: each unflagged class goes through Todd–Coxeter over ⟨x⟩, ⟨y⟩, ⟨xy⟩, ⟨xy⁻¹⟩.
   * Index 1 means the group is cyclic, hence trivial because it is perfect.
   * Index k > 1 means the coset action is a transitive degree-k permutation representation, which is output as a
     certificate.
   * The fallback, a search for homomorphisms to S₅…S₈, was never needed up to length 16.
4. `acenum cert` writes tree-edge certificates for all AC-trivial classes of length ≤ 16.
5. `acenum resolve --cap 28` covers the residual trivial-group classes. It runs BFS from the named seeds, then from each
   still-unassigned residual. A BFS stops early if it meets the cap-22 trivial component; in that case the seed is
   certified AC-trivial via the meeting state (48 components, 122 classes). Otherwise the whole capped component is
   explored and tree edges to every residual inside it are emitted.
6. `check.py` is the independent checker:
   1. It replays all 68 688 edges with `verifier.core.apply_move` and builds the union-find structure.
   2. It verifies all 67 permutation certificates.
   3. It re-enumerates all necklace pairs of length ≤ 16 with det ±1, using its own canonical form (a different order
      and different code). It checks that each class is certified AC-trivial, certified nontrivial, or in a certified
      residual component, and that the per-length class counts match the C++ enumeration exactly.

   Output: `data/check16.log`. Runtime is about 3 minutes.

Sanity checks:
* Class counts from the C++ and Python enumerations agree at every length.
* Length ≤ 13 reproduces [MM] and [HR].
* The known equivalences of [C26] are reproduced.
* All 960 trivial-group verdicts agree with SymPy.
* Our separate beam-search solver (`~/acc/acsolver`, 240 s per instance with ball14) also fails on all six named
  presentations, consistent with them being hard.

## 4. Limitations

* The result is exhaustive only for rank 2 and total length ≤ 16. The residual components at 15–16 are listed but only
  partly identified. 281 of the 287 components are not linked to the six named ones within cap 28.
* The AC-equivalences hold up to a signed letter permutation φ. This is harmless for AC-triviality and for AK3
  (Panteleev–Ushakov), but it means Theorem A does not literally name six AC classes. Running `resolve` with `--nosym`
  would remove φ at 8× the cost.
* There is no Lean formalisation. The certificates are simple enough to be checked in Lean against `AC.lean`, with
  each EDGE line becoming a `Reachable` chain. The finite enumeration (completeness) would need a verified enumerator
  or `decide`-style computation. This is future work.
* The trivial-group status of the residual presentations relies on coset enumeration, which is well established and
  cross-checked by two implementations but is not certificate-checked here.
* Novelty is asserted only to the best of our literature search. See the caveats in Section 2.

## 5. Next steps

1. Attack C3, C4, C6 (and P1, P2) with longer searches, stable moves, or the RL/greedy solvers of [S24, F26]. A
   trivialisation of any of them shrinks the list in Theorem A. A proof that C4 or C6 is AC-equivalent to AK(3) would
   be equally valuable.
2. Push the exhaustive statement to length 17 (the enumeration is done: `data/classes17_c22.txt` has 5812
   trivial-group residual classes at length 17, before component merging).
3. Formalise the certificate checker in Lean 4, so that Theorem A at length ≤ 13 or 14 becomes a Lean theorem modulo
   the enumeration.

## References

* [AC] J. J. Andrews, M. L. Curtis, Free groups and handlebodies, Proc. AMS 16 (1965) 192–195.
* [AK] S. Akbulut, R. Kirby, A potential smooth counterexample in dimension 4 to the Poincaré conjecture…, Topology 24 (1985).
* [MM] A. D. Miasnikov, A. G. Myasnikov, Balanced presentations of the trivial group on two generators and the Andrews–Curtis conjecture, Groups and Computation III (2001) 257–263; arXiv:math/0304305.
* [My] A. D. Myasnikov, Genetic algorithms and the Andrews–Curtis conjecture, IJAC 9 (1999).
* [HR] G. Havas, C. Ramsay, Breadth-first search and the Andrews–Curtis conjecture, IJAC 13(1) (2003) 61–68, doi:10.1142/S0218196703001365.
* [BM] R. S. Bowman, S. B. McCaul, Fast searching for Andrews–Curtis trivializations, Experimental Math. 15(2) (2006) 193–198.
* [PU] D. Panteleev, A. Ushakov, Conjugacy search problem and the Andrews–Curtis conjecture, arXiv:1609.00325.
* [J] D. L. Johnson, Topics in the Theory of Group Presentations, LMS LN 42, CUP (1980).
* [MS] C. F. Miller III, P. E. Schupp, Some presentations of the trivial group, Contemp. Math. 250 (1999).
* [S24] A. Shehper et al., What makes math problems hard for reinforcement learning: a case study, arXiv:2408.15332.
* [L25] A. Lisitsa, Stable Andrews–Curtis trivialization of AK(3) revisited, arXiv:2501.18601.
* [F26] L. Fagan et al., The Two-Hump Problem…, ICML 2026, arXiv:2606.21611 (AC-19 dataset).
* [C26] J. Carreras, Machine-checkable equivalence certificates at the length-14 Andrews–Curtis frontier, arXiv:2607.23611.
* SAIR ACC competition repository: `tools/verifier` (official ac-r2-v1 move semantics, used for replay), `tools/lean/AC.lean` (definitions).
* SymPy 1.14 (coset enumeration cross-check).

## Files

* `acenum.cpp`: orbit-graph BFS, enumeration, certificate emission (`bfs`, `class`, `cert`, `resolve`, `label`).
* `grp.cpp`: Todd–Coxeter plus permutation-quotient search.
* `check.py`: independent checker (official verifier + own enumeration).
* `sympy_crosscheck.py`: cross-check of the trivial-group verdicts.
* `reproduce.sh`: full pipeline; `sh reproduce.sh check` re-checks the shipped certificates only.
* `certificates/`:
  * `cert_triv16.txt`: 64 731 lines, tree to (x,y).
  * `cert_hits16_c28.txt`: edges linking the 48 trivialised residual seeds.
  * `cert_residual16_c28.txt`: SEED/EDGE lines of the residual components.
  * `groups16.txt`: permutation certificates and TC verdicts.
* `data/`: `check16.log`, `residual_components16_c28.txt`, `residual_classes16.txt`, `classes17_c22.txt`,
  `sympy_crosscheck16.txt`, `lab_c28.txt` (named MS/Johnson presentations vs. components), `lab_c46_c30.txt`.
* `work/`: intermediate files, including `comp22.bin` (424 MB, can be regenerated).
