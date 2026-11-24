"""Tests for the Binairo CSP solver."""
from pathlib import Path

import pytest

from binairo_solver import BLACK, build_problem, read_board, solution_to_board

EXAMPLES_DIR = Path(__file__).resolve().parents[1] / "binairo" / "examples"


def solve(board):
    return [solution_to_board(s, len(board)) for s in build_problem(board).getSolutions()]


def follows_rules(board):
    """Checks a filled board against the rules directly, independently of the
    solver's constraints: balanced rows and columns, no run of three."""
    size = len(board)
    lines = board + [list(column) for column in zip(*board)]
    for line in lines:
        if None in line or line.count(BLACK) != size // 2:
            return False
        if any(line[i] == line[i + 1] == line[i + 2] for i in range(size - 2)):
            return False
    return True


@pytest.mark.parametrize("filename, expected_solutions", [
    ("unique_6x6.txt", 1),
    ("empty_4x4.txt", 90),
    ("impossible_run.txt", 0),
    ("impossible_balance.txt", 0),
])
def test_example_boards_have_known_solution_counts(filename, expected_solutions):
    board = read_board(EXAMPLES_DIR / filename)
    assert len(solve(board)) == expected_solutions


@pytest.mark.parametrize("filename", ["unique_6x6.txt", "empty_4x4.txt"])
def test_every_solution_follows_the_rules_and_keeps_the_given_cells(filename):
    board = read_board(EXAMPLES_DIR / filename)
    for solution in solve(board):
        assert follows_rules(solution)
        for given_row, solved_row in zip(board, solution):
            for given, solved in zip(given_row, solved_row):
                assert given is None or given == solved


@pytest.mark.parametrize("content, error", [
    ("...\n.X.\n...\n", "even size"),
    ("..X.\n.A..\n....\n....\n", "invalid character"),
    ("....\n...\n....\n....\n", "expected 4"),
    ("", "empty"),
])
def test_invalid_boards_are_rejected(tmp_path, content, error):
    path = tmp_path / "board.txt"
    path.write_text(content)
    with pytest.raises(ValueError, match=error):
        read_board(path)