#!/usr/bin/env python3
"""Binairo solver modelled as a constraint satisfaction problem (CSP).

Rules: every row and column holds as many white as black discs, and no
three consecutive cells in a row or column share the same colour.
"""
import argparse
import sys

from constraint import ExactSumConstraint, Problem

WHITE = 0
BLACK = 1

# Characters accepted in input files, and the one used for each value on output.
EMPTY_CHARS = {".", "-"}
WHITE_CHARS = {"O", "o", "0"}
BLACK_CHARS = {"X", "x"}
SYMBOL = {None: ".", WHITE: "O", BLACK: "X"}

RUN_LENGTH = 3  # forbidden number of equal consecutive discs


def parse_cell(char, line_number):
    if char in EMPTY_CHARS:
        return None
    if char in WHITE_CHARS:
        return WHITE
    if char in BLACK_CHARS:
        return BLACK
    raise ValueError(
        f"Line {line_number}: invalid character '{char}'. Use X, O or '.'."
    )


def read_board(path):
    """Reads and validates a board file. Returns a list of rows, where each
    cell is WHITE, BLACK or None (empty)."""
    with open(path, "r", encoding="utf-8") as file:
        lines = [line.strip() for line in file if line.strip()]

    if not lines:
        raise ValueError("The file is empty.")

    size = len(lines)
    # Equal numbers of each colour per row are only possible with an even size.
    if size % 2 != 0:
        raise ValueError(f"The board is {size}x{size}; Binairo needs an even size.")

    board = []
    for line_number, line in enumerate(lines, start=1):
        if len(line) != size:
            raise ValueError(
                f"Line {line_number} has {len(line)} cells; expected {size}."
            )
        board.append([parse_cell(char, line_number) for char in line])
    return board


def format_board(board):
    size = len(board)
    border = "+" + "-" * size + "+"
    rows = ["|" + "".join(SYMBOL[cell] for cell in row) + "|" for row in board]
    return "\n".join([border, *rows, border])


def no_long_run(*cells):
    """True unless all cells have the same colour (sum 0 = all white,
    sum len(cells) = all black)."""
    return 0 < sum(cells) < len(cells)


def build_problem(board):
    size = len(board)
    problem = Problem()

    # One variable per cell. Pre-filled cells get a single-value domain.
    for row in range(size):
        for col in range(size):
            value = board[row][col]
            domain = [WHITE, BLACK] if value is None else [value]
            problem.addVariable((row, col), domain)

    # Balance: with WHITE = 0 and BLACK = 1, a line with size/2 black discs
    # sums to exactly size/2.
    half = size // 2
    for i in range(size):
        problem.addConstraint(ExactSumConstraint(half), [(i, j) for j in range(size)])
        problem.addConstraint(ExactSumConstraint(half), [(j, i) for j in range(size)])

    # Adjacency: every window of RUN_LENGTH consecutive cells, in both directions.
    for i in range(size):
        for start in range(size - RUN_LENGTH + 1):
            window = range(start, start + RUN_LENGTH)
            problem.addConstraint(no_long_run, [(i, j) for j in window])
            problem.addConstraint(no_long_run, [(j, i) for j in window])

    return problem


def solution_to_board(solution, size):
    return [[solution[(row, col)] for col in range(size)] for row in range(size)]


def write_output(path, board, solutions):
    with open(path, "w", encoding="utf-8") as file:
        file.write("Original board:\n")
        file.write(format_board(board) + "\n\n")
        if solutions:
            file.write("Solution 1:\n")
            file.write(format_board(solution_to_board(solutions[0], len(board))) + "\n")
        else:
            file.write("NO SOLUTION\n")


def main():
    parser = argparse.ArgumentParser(description="Solve a Binairo board as a CSP.")
    parser.add_argument("input", help="board file (X = black, O = white, . = empty)")
    parser.add_argument("output", help="file where the first solution is written")
    args = parser.parse_args()

    try:
        board = read_board(args.input)
    except (OSError, ValueError) as error:
        print(f"Error reading {args.input}: {error}", file=sys.stderr)
        sys.exit(1)

    size = len(board)
    print(f"Solving {size}x{size} board:")
    print(format_board(board))

    solutions = build_problem(board).getSolutions()
    if solutions:
        print(f"\n{len(solutions)} solution(s) found.")
    else:
        print("\nNo solution exists (unsatisfiable).")

    try:
        write_output(args.output, board, solutions)
    except OSError as error:
        print(f"Error writing {args.output}: {error}", file=sys.stderr)
        sys.exit(1)
    print(f"Output written to {args.output}")


if __name__ == "__main__":
    main()