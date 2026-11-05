"""Optional native kernels, with the same pure-Python word semantics."""
import functools
import os
from . import words, joins
from .shared import search as original_search

try:
    if os.environ.get('AC_SHORTCUTS_PURE_PYTHON') == '1':
        raise ImportError('Pure Python requested')
    from . import _acc_word_kernels
except ImportError:
    _acc_word_kernels = None

BACKEND = 'python' if _acc_word_kernels is None else 'native'
lev = words.lev if _acc_word_kernels is None else functools.lru_cache(maxsize=60000)(_acc_word_kernels.distance)
word_class = words.word_class if _acc_word_kernels is None else functools.lru_cache(maxsize=100000)(_acc_word_kernels.word_class)


def search(mode, *args, **kwargs):
    if mode != 'native_shared':
        raise ValueError('Expected native_shared mode')
    previous_lev, previous_class = words.lev, joins.word_class
    words.lev, joins.word_class = lev, word_class
    try:
        return original_search('shared_conjugacy', *args, **kwargs)
    finally:
        words.lev, joins.word_class = previous_lev, previous_class
