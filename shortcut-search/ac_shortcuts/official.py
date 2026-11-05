"""Load the official, separately installed Stable AC move implementation."""
import os
from pathlib import Path
import sys

tools = os.environ.get('ACC_OFFICIAL_TOOLS')
if tools:
    directory = Path(tools).expanduser().resolve()
    if not (directory / 'verifier' / 'stable_core.py').is_file():
        raise ImportError('ACC_OFFICIAL_TOOLS must point to the official competition/tools directory')
    sys.path.insert(0, str(directory))

try:
    from verifier.stable_core import apply_move, MOVE_TABLE, INVERSE_MOVE, MOVE_SPEC_VERSION
    from verifier.core import free_reduce
except ImportError as error:
    raise ImportError(
        'Install the official Andrews-Curtis checkout and set ACC_OFFICIAL_TOOLS '
        'to its competition/tools directory; see shortcut-search/README.md'
    ) from error

if MOVE_SPEC_VERSION != 'sac-r8-v1':
    raise ImportError('This search was validated against move specification sac-r8-v1')
