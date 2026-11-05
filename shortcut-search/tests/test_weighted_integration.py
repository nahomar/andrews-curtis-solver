"""Synthetic bounded-tail CLI smoke, replayed with the official move core."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from ac_shortcuts.graph import states_of

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'build'


class WeightedIntegrationTests(unittest.TestCase):
    @unittest.skipUnless(all((BUILD / name).exists() for name in
                             ('nielsen_weighted', 'test-ac.bin', 'test-sac.bin')),
                         'Run make check first to enable C++ integration')
    def test_one_state_tail_graph_preserves_paid_path(self):
        start = ((1, 2), (2,))
        old = (3, 0, 0, 16, 15)
        self.assertEqual(states_of(start, old)[-1], ())
        with tempfile.TemporaryDirectory(dir=BUILD) as directory:
            path = Path(directory)
            problems, paths, output = path / 'problems.txt', path / 'paths.jsonl', path / 'result'
            problems.write_text('ac-00000 1 2 | 2\n')
            paths.write_text(json.dumps(dict(id='sac-00000', solved=True, moves=old)) + '\n')
            command = [str(BUILD / 'nielsen_weighted'), '--problems', str(problems),
                       '--targets', str(problems), '--paths', str(paths),
                       '--incumbents', str(paths), '--seed-paths', str(paths),
                       '--out', str(output), '--ac-ball', str(BUILD / 'test-ac.bin'),
                       '--sac-ball', str(BUILD / 'test-sac.bin'), '--stable-only',
                       '--depth', '1', '--stride', '1', '--state-cap', '6',
                       '--state-limit', '1', '--tail-window', '3', '--polish-cache',
                       '--polish-radius', '1', '--candidate-slack', '2', '--seconds', '5']
            subprocess.run(command, capture_output=True, text=True, check=True, timeout=30)
            results = [json.loads(line) for file in path.glob('result*')
                       for line in file.read_text().splitlines()]
            self.assertTrue(results)
            for row in results:
                self.assertLess(len(row['moves']), len(old))
                self.assertEqual(states_of(start, row['moves'])[-1], ())


if __name__ == '__main__':
    unittest.main()
