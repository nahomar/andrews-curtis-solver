# Contributing

Build the C++ tools and run `make check`. For changes to the waypoint engines,
install `shortcut-search/` and the pinned official verifier as described in its
README, then run both the native and Python test commands. The CI workflow runs
these checks on Linux.

Include a small synthetic reproduction for correctness changes. Verify exact
states and actual atomic move counts: generator changes, relator permutations,
and temporary generators must not introduce free transitions. Replaying a
shortcut between waypoints is separate from verifying a complete challenge
submission with the official manifest.

For performance experiments, state the objective, input source, time and state
limits, and baseline before interpreting a result. Hold out whole challenges
and exact state-goal pairs from fitting and method selection. Retain unsuccessful
attempts. Report historical shortcut recovery separately from competition
records, and use fresh confirmation data before promoting a method.

Do not include credentials, saved competition paths, submission payloads, or
private training/evaluation checkpoints. Aggregate results and synthetic tests
are welcome. The standalone library performs no network requests or submissions.

This repository is MIT-licensed; retain copyright and license notices when
reusing or adapting code. Identify third-party dependencies and their provenance.
