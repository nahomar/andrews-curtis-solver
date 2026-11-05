import json
import random
import subprocess
import sys
import unittest

from ac_shortcuts import shorten
from ac_shortcuts import native, words
from ac_shortcuts.composition import compose
from ac_shortcuts.graph import forward, backward, states_of
from ac_shortcuts.official import apply_move, free_reduce
from ac_shortcuts.rewrite import prepare, rewrite
import time


class ShortcutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        prepare()

    def test_paid_rank_changes_and_both_engines(self):
        start = ((1,), (2,))
        # Synthetic witness: append/delete a fresh generator, then delete y/x.
        old = (14, 17, 16, 15)
        for engine in ('shared', 'rewritten'):
            result = shorten(start, old, budget=2, engine=engine, seconds=2)
            self.assertEqual(result['found_length'], 2)
            self.assertEqual(result['atomic_saving'], 2)
            self.assertEqual(states_of(start, result['moves'])[-1], ())
            # Rank two cannot reach the empty presentation in one atomic move.
            failed = shorten(start, old, budget=1, engine=engine, seconds=.2, expansion_cap=100)
            self.assertIsNone(failed['moves'])

    def test_exact_nonterminal_goals(self):
        for start in (((1, 2), (2,)), ((1,), (2,), (3,))):
            for move, goal in forward(start):
                old = (0, 0, move)
                result = shorten(start, old, budget=1, engine='shared', seconds=.5)
                self.assertIsNotNone(result['moves'])
                self.assertLessEqual(len(result['moves']), 1)
                self.assertEqual(states_of(start, result['moves'])[-1], goal)

    def test_every_reverse_edge_and_forward_preimage(self):
        rng = random.Random(731)
        samples = [(), ((1,),), ((-1,),), ((1,), (2,)), ((2,), (-1,)), ((1, 2), (2,)), ((1,), (2,), (3,))]
        state = samples[-1]
        for _ in range(50):
            state = rng.choice(list(forward(state)))[1]
            samples.append(state)
            if sum(map(len, state)) > 30:
                state = ((1,), (2,))
        edges = 0
        for state in samples:
            for move, predecessor in backward(state):
                self.assertEqual(apply_move(predecessor, move), (state, None))
                edges += 1
            for move, successor in forward(state):
                self.assertIn((move, state), list(backward(successor)))
        self.assertGreater(edges, 1000)
        self.assertEqual(sum(len(s) == 3 for _, s in backward(((1, 2), (2,)))), 9)

    def test_atomic_composition_and_overlap(self):
        start = ((1,), (2,))
        cycle = (14, 17)
        old = cycle * 2 + (16, 15)
        result = compose(start, old, [dict(from_index=0, to_index=2, moves=[]),
                                     dict(from_index=2, to_index=4, moves=[])])
        self.assertEqual(result['moves'], (16, 15))
        self.assertEqual(result['atomic_saving'], 4)
        overlapping = compose(start, cycle * 3, [dict(from_index=0, to_index=4, moves=[]),
                                                dict(from_index=2, to_index=6, moves=[])])
        self.assertEqual(overlapping['length'], 2)
        with self.assertRaises(ValueError):
            compose(start, old, [dict(from_index=0, to_index=1, moves=[])])

    def test_rewrite_witness(self):
        start = ((1, 2), (2,))
        old = (0, 0, 2)
        goal = states_of(start, old)[-1]
        result = rewrite(start, goal, old, 1, 1000, time.time() + 2, seconds=1)
        self.assertEqual(result['moves'], (2,))
        self.assertEqual(states_of(start, result['moves'])[-1], goal)

    def test_word_kernels_against_independent_edit_distance(self):
        def distance(a, b):
            row = list(range(len(b) + 1))
            for i, x in enumerate(a, 1):
                nxt = [i]
                for j, y in enumerate(b, 1):
                    nxt.append(min(nxt[-1] + 1, row[j] + 1, row[j - 1] + (x != y)))
                row = nxt
            return row[-1]
        rng = random.Random(29)
        for length in (0, 1, 12, 63, 64, 80):
            for _ in range(12):
                a = tuple(rng.choice((-3, -2, -1, 1, 2, 3)) for _ in range(length))
                b = tuple(rng.choice((-3, -2, -1, 1, 2, 3)) for _ in range(rng.randrange(90)))
                expected = distance(a, b)
                self.assertEqual(words.lev(a, b), expected)
                self.assertEqual(native.lev(a, b), expected)
                self.assertEqual(native.word_class(a), words.word_class(a))

    def test_reject_invalid_inputs(self):
        for state in (((0,),), ((2,),), ((1, -1),), ((True,),), ((1,),) * 4):
            with self.assertRaises(ValueError):
                shorten(state, (0, 0))
        for kwargs in ({'seconds': 0}, {'seconds': float('nan')}, {'budget': 2},
                       {'budget': -1}, {'expansion_cap': 0}, {'engine': 'unknown'}):
            with self.assertRaises(ValueError):
                shorten(((1,),), (0, 0), **kwargs)
        for moves in ((False,), (999,), (-1,), (16,)):
            with self.assertRaises(ValueError):
                shorten(((1,),), moves)

    def test_cli_and_empty_witness(self):
        result = shorten((), ())
        self.assertEqual(result['moves'], ())
        command = [sys.executable, '-B', '-m', 'ac_shortcuts', '--engine', 'shared', '--budget', '2']
        run = subprocess.run(command, input=json.dumps(dict(start=[[1], [2]], old_moves=[14, 17, 16, 15])),
                             text=True, capture_output=True, check=True)
        self.assertEqual(json.loads(run.stdout)['found_length'], 2)


if __name__ == '__main__':
    unittest.main()
