#!/usr/bin/env python3
"""Prepare bounded search inputs for near misses with exactly one record holder.

Consumes a fresh sair.py records.json snapshot and officially verified databases.
This script does not contact SAIR, start searches, or submit solutions.
"""
import argparse
import json
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--records', required=True)
    ap.add_argument('--db', nargs='+', required=True)
    ap.add_argument('--problems', required=True, help='AC puzzle definitions')
    ap.add_argument('--out-dir', required=True)
    ap.add_argument('--limit', type=int, default=24, help='maximum targets per track')
    ap.add_argument('--max-gap', type=int, default=6)
    ap.add_argument('--holders', type=int, default=1)
    ap.add_argument('--include-matches', action='store_true',
                    help='include unsubmitted paths matching the live record')
    ap.add_argument('--submitted', help='accepted-submission ledger: challenge -> length')
    a = ap.parse_args()
    if min(a.limit, a.max_gap, a.holders) < 1:
        ap.error('limit, max-gap, and holders must be positive')
    records = json.loads(Path(a.records).read_text())
    if a.include_matches and not a.submitted:
        ap.error('--include-matches requires --submitted to exclude records already held')
    submitted = json.loads(Path(a.submitted).read_text()) if a.submitted else {}
    db = {}
    for filename in a.db:
        for challenge, value in json.loads(Path(filename).read_text()).items():
            if challenge not in db or len(value['moves']) < len(db[challenge]['moves']):
                db[challenge] = value
    definitions = {}
    for line in Path(a.problems).read_text().splitlines():
        if line.strip() and not line.lstrip().startswith('#'):
            challenge, definition = line.split(maxsplit=1)
            definitions[challenge.split('-', 1)[1]] = definition
    selected = {}
    for track, offset in [('ac', 0), ('sac', 2)]:
        candidates = []
        for challenge, value in db.items():
            if not challenge.startswith(track + '-'):
                continue
            number = challenge.split('-', 1)[1]
            row = records.get('ac-' + number)
            if not row or number not in definitions:
                continue
            record, holders = row[offset:offset + 2]
            if record is None or holders != a.holders:
                continue
            length = len(value['moves'])
            accepted = submitted.get(challenge)
            if accepted is not None and accepted <= record:
                continue
            if (0 if a.include_matches else 1) <= length - record <= a.max_gap:
                candidates.append(dict(id=challenge, record=record, holders=holders,
                                       ours=length, gap=length - record))
        candidates.sort(key=lambda row: (row['gap'], row['record'], row['id']))
        selected[track] = candidates[:a.limit]
    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    seeds, numbers = [], set()
    for track, rows in selected.items():
        lines = []
        for row in rows:
            challenge = row['id']
            number = challenge.split('-', 1)[1]
            numbers.add(number)
            lines.append(f'{challenge} {definitions[number]}\n')
            seeds.append(dict(id=challenge, solved=True, moves=db[challenge]['moves']))
        (out / f'targets_{track}.txt').write_text(''.join(lines))
    (out / 'starts.txt').write_text(''.join(f'ac-{n} {definitions[n]}\n' for n in sorted(numbers)))
    (out / 'seeds.jsonl').write_text(''.join(json.dumps(row) + '\n' for row in seeds))
    (out / 'targets.json').write_text(json.dumps(selected, indent=2) + '\n')
    print(f'Selected {len(selected["ac"])} AC and {len(selected["sac"])} Stable AC '
          f'near misses with exactly {a.holders} holder(s); {len(numbers)} unique starts.')


if __name__ == '__main__':
    main()
