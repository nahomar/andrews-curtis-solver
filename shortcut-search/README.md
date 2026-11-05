# Goal-conditioned waypoint shortening

Given a valid path between two exact presentations, search for a shorter path
with the same endpoints. The endpoint can be an intermediate waypoint rather
than the empty presentation. Every ordinary move, stabilization,
destabilization, and reconstructed conjugation is charged at its actual atomic
cost. No generator renaming or relator permutation is free.

Two engines are available:

- `shared`: seed forward and reverse searches with paid prefixes and suffixes of
  the supplied witness, then try exact intersections and paid conjugacy joins.
- `rewritten`: also use universal move identities to produce alternative seeds,
  and compose compatible local shortcuts under one total query budget.

These are the standalone versions of the previously confirmed search engines.
The public packaging has synthetic correctness tests, not a new competition
performance claim. See [research findings](../docs/RESEARCH_FINDINGS.md).

## Install

Python 3.9 or later is required. A C++17 compiler enables optional native word
kernels; a pure-Python fallback is available. The official verifier is a separate
dependency and is not copied into this repository.

From the repository root:

```sh
git clone https://github.com/SAIRcompetition/Andrews-Curtis.git .official
git -C .official checkout a0fd6e6f52d82c93ccc06ab91d81e8fa3678256e
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -e ./shortcut-search
export ACC_OFFICIAL_TOOLS="$PWD/.official/competition/tools"
```

The pinned official revision uses `sac-r8-v1`. Alternatively, put its
`competition/tools` directory on `PYTHONPATH`. No account or API key is needed
to run local search or tests.

## Small example

This is an artificial four-move detour, not a competition solution:

```sh
printf '%s\n' '{"start":[[1],[2]],"old_moves":[14,17,16,15]}' |
  python -m ac_shortcuts --engine shared --budget 2
```

The two remaining moves delete the two positive basis relators. The output
includes `moves`, `found_length`, `atomic_saving`, `backend`, and search stats.
A failed bounded attempt returns `moves: null`.

```python
from ac_shortcuts import shorten

result = shorten(
    start=((1,), (2,)),
    old_moves=(14, 17, 16, 15),
    budget=2,
    engine="rewritten",
    seconds=1,
    expansion_cap=1_000_000,
)
```

For your own witness, pass a JSON file containing `start` and `old_moves` to
`python -m ac_shortcuts input.json`. Move IDs are the official Stable AC IDs.
The library verifies every returned shortcut against the exact endpoint.
Full challenge submissions still require the official manifest verifier and
server acceptance. This package never submits or updates a solution database.

## Bounds and evaluation

The graph supports ranks 0–3, words of at most 48 letters, and at most 80 letters
in a presentation. The state cap is 120,000; conjugacy buckets and sampled
anchors retain eight states each. These restrictions and heuristic priorities
can miss useful paths. A failed search is not an unreachability proof.

Time limits are cooperative, not OS-enforced deadlines. First-use rewrite-table
construction and individual operations can overrun a very small allowance.
`total_seconds` measures the serialized search call; input validation and time
waiting for another call are outside that measurement. The public API serializes
calls because the native adapters temporarily replace word kernels.

```sh
python -m unittest discover -s shortcut-search/tests -v
AC_SHORTCUTS_PURE_PYTHON=1 python -m unittest discover -s shortcut-search/tests -v
```

Tests check reverse-edge preimages against the official move implementation,
atomic costs, endpoint preservation, rewrite witnesses, composition overlap,
native/Python word semantics, invalid inputs, and the CLI. They use synthetic
examples. Private competition trajectories, model weights, and held-out datasets
are not part of the distribution.
