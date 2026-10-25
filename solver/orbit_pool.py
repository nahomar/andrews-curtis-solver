#!/usr/bin/env python3
"""Export saved databases and a bounded set of alternative paths to JSONL.

This prepares candidates, not proof certificates. orbit_splice replays its inputs
and outputs; check.py must still run the official verifier before submission.
"""
import argparse
import json
from collections import defaultdict
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--db', nargs='+', required=True, type=Path)
    parser.add_argument('--alternatives', nargs='*', default=[], type=Path)
    parser.add_argument('--max-extra', type=int, default=4)
    parser.add_argument('--slack', type=int, default=20)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    if args.max_extra < 0 or args.slack < 0:
        parser.error('--max-extra and --slack must be nonnegative')
    saved = {}
    best = {}
    for source in args.db:
        for challenge, value in json.loads(source.read_text()).items():
            moves = tuple(value['moves'])
            saved[challenge, moves] = None
            best[challenge] = min(best.get(challenge, len(moves)), len(moves))
    extra = defaultdict(set)
    malformed = 0
    for source in args.alternatives:
        with source.open() as stream:
            for line in stream:
                try:
                    row = json.loads(line)
                    challenge = row['id']
                    moves = tuple(row['moves'])
                except (ValueError, KeyError, TypeError):
                    malformed += 1
                    continue
                if (not row.get('solved') or challenge not in best
                        or len(moves) > best[challenge] + args.slack
                        or (challenge, moves) in saved):
                    continue
                extra[challenge].add(moves)
                if len(extra[challenge]) > args.max_extra:
                    extra[challenge].remove(max(extra[challenge], key=lambda p: (len(p), p)))
    with args.out.open('w') as stream:
        for challenge, moves in saved:
            stream.write(json.dumps({'id': challenge, 'solved': True, 'moves': moves}) + '\n')
        for challenge in sorted(extra):
            for moves in sorted(extra[challenge], key=lambda p: (len(p), p)):
                stream.write(json.dumps({'id': challenge, 'solved': True, 'moves': moves}) + '\n')
    print(f'{len(saved)} saved paths, {sum(map(len, extra.values()))} alternatives; '
          f'{malformed} malformed rows skipped -> {args.out}')


if __name__ == '__main__':
    main()
