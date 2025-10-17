"""
Takes all the cc and h files in echion/ and amalgamates them into a single compilable file.

Header inclusion order is respected using a topological sort, so that headers that depend on other headers are included
after them.
"""

import os
import re
import logging

from collections import defaultdict, deque
from typing import List, Dict, Tuple

logging.basicConfig()
log = logging.getLogger("amalgamate")
log.setLevel(logging.INFO)


REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
ECHION_DIR = os.path.join(REPO_ROOT, "echion")

def find_all_files(directory: str) -> List[str]:
    """Recursively find all .h and .cc files"""
    files: List[str] = []
    for root, _, file_list in os.walk(directory):
        for file in file_list:
            if file.endswith('.h') or file.endswith('.cc'):
                files.append(os.path.join(root, file))
    return files

def parse_echion_includes(file_path: str) -> List[str]:
    """Parse a file and return list of echion/ headers it includes"""
    includes: List[str] = []
    try:
        with open(file_path, 'r') as f:
            for line in f:
                # Look for #include <echion/...>
                match = re.match(r'\s*#include\s*<echion/(.+)>', line.strip())
                if match:
                    header_name = match.group(1)
                    includes.append(header_name)
    except Exception as e:
        log.exception(f"Warning: Could not read {file_path}: {e}")
        raise

    return includes

def build_dependency_graph(files: List[str]) -> Tuple[Dict[str, List[str]], Dict[str, str], List[str]]:
    """Build dependency graph for header files"""
    # Separate headers and implementation files
    headers = [f for f in files if f.endswith('.h')]
    cc_files = [f for f in files if f.endswith('.cc')]

    # Create mapping from header name to full path
    header_map: Dict[str, str] = {}
    for header_path in headers:
        # Get relative path from echion/ directory
        rel_path = os.path.relpath(header_path, ECHION_DIR)
        header_map[rel_path] = header_path

    # Build dependency graph
    dependencies = defaultdict(list)  # header -> list of headers it depends on

    for header_path in headers:
        rel_path = os.path.relpath(header_path, ECHION_DIR)
        includes = parse_echion_includes(header_path)

        for include in includes:
            if include in header_map:
                dependencies[rel_path].append(include)

    return dependencies, header_map, cc_files

def topological_sort(dependencies: Dict[str, List[str]]) -> List[str]:
    """Perform topological sort on dependency graph"""
    # Calculate in-degrees
    in_degree = defaultdict(int)
    all_nodes = set(dependencies.keys())

    # Add all dependent nodes to the set
    for deps in dependencies.values():
        all_nodes.update(deps)

    # Initialize in-degrees
    for node in all_nodes:
        in_degree[node] = 0

    # Calculate in-degrees
    for node, deps in dependencies.items():
        for _ in deps:
            in_degree[node] += 1

    # Kahn's algorithm
    queue = deque(sorted([node for node in all_nodes if in_degree[node] == 0]))
    result: List[str] = []

    while queue:
        current = queue.popleft()
        result.append(current)

        # For each node that depends on current, reduce its in-degree
        ready_nodes: List[str] = []
        for node, deps in dependencies.items():
            if current in deps:
                in_degree[node] -= 1
                if in_degree[node] == 0:
                    ready_nodes.append(node)
        
        # Add ready nodes in sorted order for deterministic tie-breaking
        for node in sorted(ready_nodes):
            queue.append(node)

    # Check for cycles
    if len(result) != len(all_nodes):
        log.warning("Dependency cycle detected!")
        # Add remaining nodes anyway
        for node in all_nodes:
            if node not in result:
                result.append(node)

    return result

def generate_file(out_file: str):
    log.info(f"Generating file {out_file}")
    # Find all files recursively
    all_files = find_all_files(ECHION_DIR)

    # Build dependency graph and get sorted order
    dependencies, header_map, cc_files = build_dependency_graph(all_files)
    header_order = topological_sort(dependencies)

    # Convert back to full paths, filtering out headers that don't exist
    h_files: List[str] = []
    for header_rel in header_order:
        if header_rel in header_map:
            h_files.append(header_map[header_rel])

    # Sort CC files alphabetically as fallback
    cc_files.sort(key=lambda x: os.path.basename(x))

    log.info(f"Found {len(h_files)} header files")
    log.info(f"Found {len(cc_files)} source files")
    log.info("Header dependency order:")
    for i, h_file in enumerate(h_files):
        rel_path = os.path.relpath(h_file, ECHION_DIR)
        deps = dependencies.get(rel_path, [])
        log.info(f"  {i+1:2d}. {rel_path} (depends on: {', '.join(deps) if deps else 'none'})")

    # Write the files to a single file
    with open(out_file, "w") as out_f:
        for file in h_files:
            with open(file) as f:
                lines = f.readlines()
                additional_lines_before = [
                    f"\n// ======================== Start of {file} ========================\n",
                ]
                additional_lines_after = [
                    f"\n// ======================== End of {file} ========================\n",
                ]

                lines = additional_lines_before + [line for line in lines if not line.startswith("#include <echion/") and  "#pragma once" not in line] + additional_lines_after

                out_f.write("".join(lines))

        for file in cc_files:
            with open(file) as f:
                lines = f.readlines()
                lines = [line for line in lines if not line.startswith("#include <echion/")]

                out_f.write("".join(lines))

if __name__ == "__main__":
    generate_file(os.path.join(REPO_ROOT, "echion.cc"))