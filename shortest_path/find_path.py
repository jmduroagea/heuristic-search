#!/usr/bin/env python3
"""Command-line wrapper for the C++ shortest-path solver.

Compiles the solver when needed, decompresses .gz map files if only the
compressed version exists, and runs the search.
"""
import argparse
import gzip
import platform
import shutil
import subprocess
import sys
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
SRC_DIR = BASE_DIR / "src"
INCLUDE_DIR = BASE_DIR / "include"

EXE_SUFFIX = ".exe" if platform.system() == "Windows" else ""
SOLVER = BASE_DIR / f"main_solver{EXE_SUFFIX}"

COMPILER = "g++"
# NDEBUG turns off the asserts, which only guard against programming errors.
COMPILE_FLAGS = ["-O3", "-std=c++17", "-DNDEBUG"]

ALGORITHMS = ["astar", "dijkstra"]
INTERRUPTED_EXIT_CODE = 130  # shell convention for Ctrl+C


def fail(message):
    print(f"Error: {message}", file=sys.stderr)
    sys.exit(1)


def needs_rebuild(sources, headers):
    """True if the solver is missing or older than any source or header."""
    if not SOLVER.exists():
        return True
    built_at = SOLVER.stat().st_mtime
    return any(path.stat().st_mtime > built_at for path in sources + headers)


def build_solver():
    sources = sorted(SRC_DIR.glob("*.cpp"))
    headers = sorted(INCLUDE_DIR.glob("*.hpp"))
    if not sources:
        fail(f"no C++ sources found in {SRC_DIR}")
    if not needs_rebuild(sources, headers):
        return

    if shutil.which(COMPILER) is None:
        fail(f"{COMPILER} not found; install it to build the solver")

    print("Compiling solver...")
    command = [COMPILER, *COMPILE_FLAGS, f"-I{INCLUDE_DIR}",
               *map(str, sources), "-o", str(SOLVER)]
    if subprocess.run(command).returncode != 0:
        fail("compilation failed")


def map_file(base, extension):
    """Returns the map file base+extension, decompressing base+extension+.gz
    first if only the compressed file exists."""
    path = Path(f"{base}{extension}")
    if path.exists():
        return path

    compressed = Path(f"{path}.gz")
    if not compressed.exists():
        fail(f"map file not found: {path} (or {compressed})")

    print(f"Decompressing {compressed}...")
    try:
        with gzip.open(compressed, "rb") as source, open(path, "wb") as target:
            shutil.copyfileobj(source, target)
    except (OSError, EOFError) as error:
        path.unlink(missing_ok=True)  # do not leave a half-written map behind
        fail(f"cannot decompress {compressed}: {error}")
    return path


def main():
    parser = argparse.ArgumentParser(
        description="Find the shortest path between two nodes of a DIMACS road map.")
    parser.add_argument("source", help="source node id (1-based)")
    parser.add_argument("target", help="target node id (1-based)")
    parser.add_argument("map", help="map path without extension, e.g. maps/USA-road-d.NY")
    parser.add_argument("output", help="file where the path is written")
    parser.add_argument("algorithm", nargs="?", default="astar", choices=ALGORITHMS)
    args = parser.parse_args()

    build_solver()
    graph = map_file(args.map, ".gr")
    coords = map_file(args.map, ".co")

    command = [str(SOLVER), str(graph), str(coords),
               args.source, args.target, args.output, args.algorithm]
    try:
        sys.exit(subprocess.run(command).returncode)
    except KeyboardInterrupt:
        sys.exit(INTERRUPTED_EXIT_CODE)


if __name__ == "__main__":
    main()