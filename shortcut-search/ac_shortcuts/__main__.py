import argparse
import json
import sys
from . import shorten


def main():
    parser = argparse.ArgumentParser(description='Shorten a supplied Stable AC waypoint path; never submits.')
    parser.add_argument('input', nargs='?', default='-', help='JSON file with start and old_moves; - reads stdin')
    parser.add_argument('--engine', choices=['shared', 'rewritten'], default='rewritten')
    parser.add_argument('--seconds', type=float, default=1.0)
    parser.add_argument('--budget', type=int)
    parser.add_argument('--expansion-cap', type=int, default=1_000_000)
    args = parser.parse_args()
    try:
        if args.input == '-':
            request = json.load(sys.stdin)
        else:
            with open(args.input) as stream:
                request = json.load(stream)
        result = shorten(request['start'], request['old_moves'], budget=args.budget,
                         engine=args.engine, seconds=args.seconds, expansion_cap=args.expansion_cap)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    print(json.dumps(result))


if __name__ == '__main__':
    main()
