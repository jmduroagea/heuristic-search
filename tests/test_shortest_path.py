"""Black-box tests for the C++ shortest-path solver.

They run on a small synthetic road network generated on the fly, so they need
no map downloads and finish in seconds.
"""
import math
import random
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

import pytest

import find_path

SEED = 2024
NUM_RANDOM_PAIRS = 50

# Synthetic network: a jittered grid of intersections about 200 m apart.
GRID_SIZE = 40
SPACING_MICRODEG = 2000
JITTER_MICRODEG = 600
ORIGIN_LON, ORIGIN_LAT = -3_700_000, 40_400_000  # microdegrees
ONE_WAY_PROBABILITY = 0.1
DIAGONAL_PROBABILITY = 0.3
DETOUR_RANGE = (1.0, 1.3)  # road length / straight-line distance
DECIMETRES_PER_METRE = 10  # DIMACS distance graphs use decimetres
EARTH_RADIUS_M = 6_371_000


@dataclass
class RoadGraph:
    gr: Path
    co: Path
    num_nodes: int
    cheapest_edge: dict  # (u, v) -> cost, 1-based ids
    isolated_node: int   # node with no edges, for "no path" tests


def metres_between(a, b):
    lat1, lat2 = math.radians(a[1] / 1e6), math.radians(b[1] / 1e6)
    dlon = math.radians((b[0] - a[0]) / 1e6)
    h = (math.sin((lat2 - lat1) / 2) ** 2
         + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2) ** 2)
    return 2 * EARTH_RADIUS_M * math.asin(math.sqrt(h))


def generate_road_graph(directory, rng):
    coords = [
        (ORIGIN_LON + col * SPACING_MICRODEG + rng.randint(-JITTER_MICRODEG, JITTER_MICRODEG),
         ORIGIN_LAT + row * SPACING_MICRODEG + rng.randint(-JITTER_MICRODEG, JITTER_MICRODEG))
        for row in range(GRID_SIZE) for col in range(GRID_SIZE)
    ]
    edges = []

    def connect(u, v):
        cost = math.ceil(metres_between(coords[u], coords[v]) * DECIMETRES_PER_METRE
                         * rng.uniform(*DETOUR_RANGE))
        directions = [(u, v), (v, u)]
        if rng.random() < ONE_WAY_PROBABILITY:
            directions = [rng.choice(directions)]
        edges.extend((a, b, cost) for a, b in directions)

    for row in range(GRID_SIZE):
        for col in range(GRID_SIZE):
            node = row * GRID_SIZE + col
            if col + 1 < GRID_SIZE:
                connect(node, node + 1)
            if row + 1 < GRID_SIZE:
                connect(node, node + GRID_SIZE)
            if col + 1 < GRID_SIZE and row + 1 < GRID_SIZE and rng.random() < DIAGONAL_PROBABILITY:
                connect(node, node + GRID_SIZE + 1)

    coords.append((ORIGIN_LON, ORIGIN_LAT - 10 * SPACING_MICRODEG))  # isolated node
    num_nodes = len(coords)

    gr = directory / "synthetic.gr"
    co = directory / "synthetic.co"
    with open(gr, "w") as f:
        f.write(f"p sp {num_nodes} {len(edges)}\n")
        f.writelines(f"a {u + 1} {v + 1} {cost}\n" for u, v, cost in edges)
    with open(co, "w") as f:
        f.writelines(f"v {i + 1} {lon} {lat}\n" for i, (lon, lat) in enumerate(coords))

    cheapest = {}
    for u, v, cost in edges:
        key = (u + 1, v + 1)
        cheapest[key] = min(cost, cheapest.get(key, cost))
    return RoadGraph(gr, co, num_nodes, cheapest, isolated_node=num_nodes)


@pytest.fixture(scope="session")
def solver():
    find_path.build_solver()
    return find_path.SOLVER


@pytest.fixture(scope="session")
def graph(tmp_path_factory):
    return generate_road_graph(tmp_path_factory.mktemp("maps"), random.Random(SEED))


def run_solver(solver, graph, source, target, algorithm, output):
    return subprocess.run(
        [str(solver), str(graph.gr), str(graph.co), str(source), str(target),
         str(output), algorithm],
        capture_output=True, text=True)


def reported_cost(stdout):
    match = re.search(r"Optimal path cost: (\d+)", stdout)
    return int(match.group(1)) if match else None


def read_path(output):
    """Parses "1 - (1498) - 308 - (8718) - 309" into nodes and edge costs."""
    parts = output.read_text().strip().split(" - ")
    nodes = [int(p) for p in parts[0::2]]
    costs = [int(p.strip("()")) for p in parts[1::2]]
    return nodes, costs


def test_astar_matches_dijkstra_on_random_pairs(solver, graph, tmp_path):
    rng = random.Random(SEED)
    mismatches = []
    for _ in range(NUM_RANDOM_PAIRS):
        source, target = rng.randint(1, graph.num_nodes - 1), rng.randint(1, graph.num_nodes - 1)
        astar = reported_cost(run_solver(solver, graph, source, target, "astar",
                                         tmp_path / "a.txt").stdout)
        dijkstra = reported_cost(run_solver(solver, graph, source, target, "dijkstra",
                                            tmp_path / "d.txt").stdout)
        if astar != dijkstra:
            mismatches.append((source, target, astar, dijkstra))
    assert not mismatches


@pytest.mark.parametrize("algorithm", ["astar", "dijkstra"])
def test_output_path_is_a_real_path_with_the_reported_cost(solver, graph, tmp_path, algorithm):
    source, target = 1, GRID_SIZE * GRID_SIZE  # opposite corners of the grid
    output = tmp_path / "path.txt"
    result = run_solver(solver, graph, source, target, algorithm, output)
    assert result.returncode == 0

    nodes, costs = read_path(output)
    assert nodes[0] == source and nodes[-1] == target
    for u, v, cost in zip(nodes, nodes[1:], costs):
        assert graph.cheapest_edge.get((u, v)) == cost
    assert sum(costs) == reported_cost(result.stdout)


def test_same_source_and_target_costs_zero(solver, graph, tmp_path):
    result = run_solver(solver, graph, 5, 5, "astar", tmp_path / "path.txt")
    assert reported_cost(result.stdout) == 0


@pytest.mark.parametrize("algorithm", ["astar", "dijkstra"])
def test_unreachable_target_reports_no_path(solver, graph, tmp_path, algorithm):
    output = tmp_path / "path.txt"
    result = run_solver(solver, graph, 1, graph.isolated_node, algorithm, output)
    assert result.returncode == 0
    assert "No path found" in result.stdout
    assert not output.exists()


@pytest.mark.parametrize("bad_id", ["0", "-1", "abc", "12abc", "too_big"])
def test_invalid_node_ids_are_rejected(solver, graph, tmp_path, bad_id):
    if bad_id == "too_big":
        bad_id = str(graph.num_nodes + 1)
    result = run_solver(solver, graph, bad_id, 1, "astar", tmp_path / "path.txt")
    assert result.returncode == 1
    assert "node ids must be integers" in result.stderr


def test_unknown_algorithm_is_rejected(solver, graph, tmp_path):
    result = run_solver(solver, graph, 1, 2, "bfs", tmp_path / "path.txt")
    assert result.returncode == 1
    assert "unknown algorithm" in result.stderr