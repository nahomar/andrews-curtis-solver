"""Compose compatible, exactly replayed shortcuts without double-counting savings."""
from .graph import states_of

def compose(start, old_moves, shortcuts):
    old = tuple(old_moves)
    states = states_of(start, old)
    outgoing = [[] for _ in states]
    for (i, move) in enumerate(old):
        outgoing[i].append((i + 1, (move,), None))
    for (edge_index, edge) in enumerate(shortcuts):
        (i, j) = (edge['from_index'], edge['to_index'])
        moves = tuple(edge['moves'])
        if type(i) is not int or type(j) is not int or (not 0 <= i < j <= len(old)):
            raise ValueError('Shortcut must advance within the frozen trajectory')
        if len(moves) >= j - i:
            raise ValueError('Shortcut must strictly reduce atomic move count')
        if states_of(states[i], moves)[-1] != states[j]:
            raise ValueError('Shortcut does not reach the exact indexed goal')
        outgoing[i].append((j, moves, edge_index))
    distances = [float('inf')] * len(states)
    previous = [None] * len(states)
    distances[0] = 0
    for (i, edges) in enumerate(outgoing):
        for (j, moves, edge_index) in edges:
            cost = distances[i] + len(moves)
            if cost < distances[j]:
                distances[j] = cost
                previous[j] = (i, moves, edge_index)
    (pieces, selected, at) = ([], [], len(old))
    while at:
        (i, moves, edge_index) = previous[at]
        pieces.append(moves)
        if edge_index is not None:
            selected.append(edge_index)
        at = i
    answer = tuple((m for piece in reversed(pieces) for m in piece))
    assert len(answer) == distances[-1]
    assert states_of(start, answer)[-1] == states[-1]
    return dict(moves=answer, length=len(answer), atomic_saving=len(old) - len(answer), selected_shortcut_indices=list(reversed(selected)), supplied_shortcuts=len(shortcuts))
