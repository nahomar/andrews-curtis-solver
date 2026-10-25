#!/usr/bin/env python3
"""Prepare rank-three bridge seeds from fully replayed Stable AC paths.

Requires the official verifier checkout. Each seed stores the path to a rank-two
state and its complete verified suffix. The C++ search charges stabilization
and destabilization itself. This program does not submit or update databases.
"""
import argparse
import json
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--official-tools', required=True)
    ap.add_argument('--problems', required=True)
    ap.add_argument('--paths', nargs='+', required=True)
    ap.add_argument('--id', required=True)
    ap.add_argument('--cap', type=int, default=48)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    if not 3 <= a.cap <= 64:
        ap.error('--cap must be between 3 and 64')
    sys.path.insert(0, a.official_tools)
    from verifier.stable_core import apply_move
    starts = {}
    for line in Path(a.problems).read_text().splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        words = line.split()
        split = words.index('|')
        starts[words[0].split('-', 1)[1]] = (
            tuple(map(int, words[1:split])), tuple(map(int, words[split + 1:])))
    start = starts[a.id.split('-', 1)[1]]
    rows, verified = [], 0
    for filename in a.paths:
        for line in Path(filename).read_text().splitlines():
            row = json.loads(line)
            if row.get('id') != a.id or not row.get('solved'):
                continue
            moves, state = row['moves'], start
            route_rows = []
            for i, move in enumerate(moves):
                if len(state) == 2 and sum(map(len, state)) + 1 <= a.cap:
                    values = [i, len(moves) - i, len(state[0]), len(state[1]),
                              *moves[:i], *moves[i:], *state[0], *state[1]]
                    route_rows.append(' '.join(map(str, values)))
                state, error = apply_move(state, move)
                if error:
                    raise ValueError(f'{a.id}: invalid move at {i}: {error}')
            if state != ():
                raise ValueError(f'{a.id}: path does not reach the empty presentation')
            verified += 1
            rows.extend(route_rows)
    if not verified:
        raise ValueError(f'No valid routes found for {a.id}')
    Path(a.out).write_text('\n'.join(rows) + '\n')
    print(f'{verified} fully replayed routes; {len(rows)} seed rows')


if __name__ == '__main__':
    main()
