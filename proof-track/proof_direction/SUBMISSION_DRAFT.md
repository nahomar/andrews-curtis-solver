# SAIR ACC Proof Track: submission draft (NOT SUBMITTED; needs the user's approval)

**Conjecture:** AC
**Direction:** proof
**Public GitHub repository link:** (optional; none yet. There is no Lean formalisation. If the certificate repository
is published, add the link and the full commit hash.)
**arXiv / paper link:** (none yet)
**Sharing agreement:** to be ticked by the submitter.

---

## Description

**Title:** An exhaustive, certified reduction of the rank-2 AC conjecture at total length ≤ 14 (and ≤ 16) to finitely
many explicit presentations.

**Result (computer-verified, with checkable certificates).** Let ⟨x,y | r₁, r₂⟩ be a balanced presentation of the
trivial group whose relators have total freely reduced length ≤ 14. Then it is AC-trivial, or it is AC-equivalent to
φ(Q), where φ is one of the 8 signed letter permutations of F(x,y) and Q is one of six presentations:

* AK(3) = ⟨x,y | x³y⁻⁴, xyxy⁻¹x⁻¹y⁻¹⟩;
* Johnson's P1 = ⟨x,y | x⁻¹y²xy⁻³, y⁻¹x²yx⁻³⟩;
* P2 = MS(2, yx²yx⁻²);
* C3 = ⟨x,y | x²yx⁻¹y, xy⁵xy⁻²⟩;
* C4 = ⟨x,y | x⁴yx⁻¹y, xy³xy⁻²⟩;
* C6 = ⟨x,y | x²yx²y⁻², xyx⁻¹y⁻²x⁻¹y⟩ (relators `xxyxxYY`, `xyXYYXy`).

Since φ preserves AC-triviality, AC holds for all rank-2 balanced presentations of length ≤ 14 if and only if it holds
for these six. Up to relator rotation, inversion, swap and φ, length 14 has exactly 4070 classes with trivial
abelianisation:
* 4052 are certified AC-trivial;
* 4 are certified to present nontrivial groups (explicit permutation quotients);
* 14 are certified AC-equivalent (up to φ) to the six presentations above.

The same computation reproduces the known results at length ≤ 12 (Miasnikov–Myasnikov: all AC-trivial) and at length
13 (Havas–Ramsay: AC-trivial or AK(3)). At lengths 15 and 16 we get the analogous exhaustive statement with 76 and 265
certified residual components respectively (287 in total up to length 16). Every length-≤16 presentation of the trivial
group is certified AC-trivial or lies in one of them.

**How it is established.**
1. *By hand:* reduction lemmas. Cyclic reduction, rotation, inversion and swapping of relators are AC operations.
   Automorphisms of F₂ map AC-sequences to AC-sequences and fix the class of the standard presentation. Trivial group
   implies the exponent-sum determinant is ±1. So it suffices to enumerate cyclically reduced pairs up to symmetry.
2. *Computation:*
   * A breadth-first search of the graph of symmetry classes of total length ≤ 22, starting from (x, y), gives
     21.2 M classes.
   * An exhaustive enumeration of all det ±1 classes up to length 16.
   * Todd–Coxeter / permutation quotients to separate out nontrivial groups.
   * BFS at total length ≤ 28 from the residuals to group them into components.
3. *Certificates:*
   * 68 688 edges, each an explicit sequence of official `ac-r2-v1` moves plus a letter permutation. They are replayed
     with the organisers' verifier code (`tools/verifier/core.py`).
   * 67 permutation representations that certify nontrivial groups.
   * An independent Python checker with its own canonical form re-enumerates all classes up to length 16 and confirms
     that every class is accounted for. The class counts match the C++ enumeration exactly.

**What is not claimed.**
* We do not claim that the six presentations are pairwise AC-inequivalent or AC-nontrivial. We only know that their
  components in the length-capped search graph stay disjoint up to total length 28 (C4 and C6 up to 30).
* That the residual presentations present the trivial group is established by coset enumeration and cross-checked with
  SymPy. It is not part of the certificates, and it is not needed for the statement above.
* There is no Lean formalisation yet.

**Novelty (to our knowledge).** Carreras (arXiv:2607.23611, 2026) notes that length 14 had never been exhaustively
classified. Our result is such an exhaustive classification (up to the six residual classes), and it extends to
lengths 15–16. It is consistent with and independently re-derives the known length-14 equivalences among Johnson's P1
and the six hard Miller–Schupp presentations of Shehper et al. It also exhibits three further length-14 presentations,
C3, C4 and C6, that are not in the capped components of AK(3), P1 or P2. We could not access the full text of
Bowman–McCaul (2006), and we have not compared against the unsolved part of the AC-19 dataset (Fagan et al. 2026). C3,
C4 and C6 may therefore appear unlabelled in earlier data.

**Prior work used and cited.**
* Andrews–Curtis (1965): the conjecture.
* Akbulut–Kirby: AK(n).
* Miasnikov–Myasnikov (2001, arXiv:math/0304305) and Havas–Ramsay (IJAC 2003): the length ≤ 12 and length 13
  results, which we reproduce; the GS-style search graph follows these papers.
* Bowman–McCaul (Exp. Math. 2006): context.
* Panteleev–Ushakov (arXiv:1609.00325): the automorphism invariance of AK(n), used to drop φ for AK(3).
* Johnson (1980): P1.
* Miller–Schupp (1999): the MS(n,w) family.
* Shehper et al. (arXiv:2408.15332): the hard length-14 MS presentations; stable triviality of AK(3).
* Lisitsa (arXiv:2501.18601): stable triviality of AK(3).
* Fagan et al. (ICML 2026, arXiv:2606.21611): the AC-19 enumeration.
* Carreras (arXiv:2607.23611): the length-14 frontier and its certified equivalences.
* SAIR ACC repository: the official verifier and `AC.lean`.
* SymPy: coset enumeration cross-check.

All code, certificates and logs: `acenum.cpp`, `grp.cpp`, `check.py`, `certificates/`, `data/` (to be published in a
public repository before submission).

**Open problems this leaves.** AC-trivialise (or prove AC-equivalent to AK(3)) any of P1, P2, C3, C4, C6. Push the
exhaustive statement to length 17 and beyond. Formalise the certificate check in Lean against `AC.lean`.
