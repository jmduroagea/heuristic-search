# Heuristic Search: Binairo CSP and Shortest Paths on the USA Road Network

Two search problems solved with exact methods:

1. **Binairo** (also known as Takuzu), modelled as a constraint satisfaction problem in Python.
2. **Optimal shortest paths** on the full USA road network of the 9th DIMACS Implementation Challenge (23.9 million nodes, 58.3 million arcs), using bidirectional A\* in C++.

## Results

Query from node 1 (Alabama) to node 23,947,347 (Wyoming), full USA distance graph:

| Algorithm | Nodes expanded | Search time |
|---|---:|---:|
| Dijkstra | 19,259,382 | 4.62 s |
| Bidirectional A\*, first version | 29,459,793 | 8.18 s |
| **Bidirectional A\*, current** | **6,753,413** | **2.15 s** |

All three return the same optimal cost (23,228,284). Over 8 random pairs on the same graph, the current A\* expanded between 1.6 and 5.6 times fewer nodes than Dijkstra, always with the same optimal cost.

## Shortest paths: how it works

**Loading.** Each DIMACS file is read into memory with a single read and parsed by a minimal number parser, avoiding line-by-line I/O on files of over 1 GB. The graph is stored as forward and reverse adjacency lists, so the backward search can walk edges against their direction.

**Open list.** A 4-ary min-heap over a flat vector with lazy deletion. The four children of a node are contiguous in memory, and the tree is half as tall as a binary heap.

**Heuristic: calibrated chord distance.** Each node is mapped to a point on the unit sphere, and the estimate between two nodes is the straight-line (chord) distance between them, multiplied by a scale factor. The chord is a true distance (it obeys the triangle inequality) and needs no trigonometry at query time. The scale is computed from the data when the graph loads: it is the largest value such that the estimate does not exceed the cost of any edge. This makes the heuristic consistent by construction, whatever unit the costs use. On the USA distance graph it comes out at 6.68 cost units per metre.

**Search: bidirectional A\* with balanced potentials** (Ikeda et al., 1994; Goldberg and Harrelson, 2005). Both searches use the average of "estimate to target" and "minus estimate to source", so they are pulled towards each other and meet in the middle. The forward and backward potentials of every node add up to a constant, which gives an exact stopping rule: once the two smallest priorities can no longer beat the best complete path found, the search stops with a proven optimum.

### What changed when I revisited the project

The first version (above, 29.5 M expansions) was correct but slower than plain Dijkstra. Measuring it against Dijkstra on the full graph exposed three issues:

1. **Stopping rule.** The first version stopped when the *sum* of both frontiers' smallest priorities reached the best path found, which can return non-optimal paths. Pohl's criterion (stop when *either* one does) fixed it; the balanced potentials later replaced both.
2. **Units.** The DIMACS distance graphs measure arcs in decimetres, not metres (median: 9.994 weight units per metre over all 58 M arcs). The heuristic estimated in metres, so it was 10 times too weak, and A\* behaved like Dijkstra while paying for a heuristic on every edge. A fixed factor of 10 would overestimate most arcs slightly because of rounding, so the scale is now calibrated from the data instead.
3. **Guidance.** Even with a correct, stronger heuristic, each search was pulled towards its own goal and overshot the middle. Balanced potentials solved this.

The test suite now checks A\* against Dijkstra on random pairs, and fails on both kinds of bug.

## Binairo: the model

Each cell is a variable with domain {0, 1} (white, black); given cells get a single-value domain. Two families of constraints, using the `python-constraint` library:

- **Balance:** every row and column sums to n/2 (`ExactSumConstraint`). This also forces n to be even, which the input validation checks.
- **No runs of three:** every window of three consecutive cells, in both directions, sums to 1 or 2.

The solver reports how many solutions exist and writes the first one.

## Usage

Requirements: Python 3.8+, and `g++` with C++17 support (on Windows, for example through MSYS2).

```bash
python -m venv .venv
.venv/Scripts/python -m pip install -r requirements.txt   # Linux/macOS: .venv/bin/python
```

**Binairo.** Boards are plain text files: `X` for black, `O` for white, `.` for empty.

```bash
python binairo/binairo_solver.py binairo/examples/unique_6x6.txt solution.txt
```

**Shortest path.** The wrapper compiles the solver when the sources change, and decompresses `.gz` maps on first use.

```bash
python shortest_path/find_path.py <source> <target> <map> <output> [astar|dijkstra]
python shortest_path/find_path.py 1 23947347 shortest_path/maps/USA-road-d.USA path.txt
```

The output file lists the path as `1 - (2025) - 2 - (1305) - ...`: node ids with the cost of each arc in between.

### Getting the maps

The maps are too large for this repository. The original DIMACS download page is no longer online, but the Internet Archive keeps copies of the files (public domain). Put them in `shortest_path/maps/`:

- [USA-road-d.USA.gr.gz](https://web.archive.org/web/20231119052609id_/http://www.diag.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.gr.gz) (335 MB)
- [USA-road-d.USA.co.gz](https://web.archive.org/web/20161221085623id_/http://www.dis.uniroma1.it/challenge9/data/USA-road-d/USA-road-d.USA.co.gz) (218 MB)

Loading the full USA graph needs several GB of RAM.

## Tests

```bash
python -m pytest
```

The shortest-path tests run the compiled solver on a synthetic road network generated on the fly (a jittered grid with one-way streets), so they need no downloads and take a few seconds. They check that A\* and Dijkstra agree on 50 random pairs, that every output path is made of real arcs adding up to the reported cost, and the edge cases (same source and target, unreachable target, invalid input). The Binairo tests check known solution counts, validate every solution against the rules independently of the solver, and cover malformed boards.

## Project structure

```
binairo/
  binairo_solver.py      CSP model and command-line interface
  examples/              sample boards
shortest_path/
  find_path.py           build-and-run wrapper
  include/, src/         C++ solver (graph, open list, closed list, path finder)
  maps/                  DIMACS maps go here (not tracked)
tests/                   pytest suite
```

## About

By José María Duro Agea. Started as coursework for *Heurística y Optimización* at Universidad Carlos III de Madrid (2025), and revisited in 2026: code cleanup, the fixes described above, and the test suite.

**References**
- I. Pohl, "Bi-directional search", *Machine Intelligence* 6, 1971.
- T. Ikeda et al., "A fast algorithm for finding better routes by AI search techniques", *Vehicle Navigation and Information Systems Conference*, 1994.
- A. V. Goldberg and C. Harrelson, "Computing the shortest path: A\* search meets graph theory", *SODA\*, 2005.
- Data: 9th DIMACS Implementation Challenge on Shortest Paths.