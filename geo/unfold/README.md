# Unfolding Logic

This directory contains the core logic for unfolding 3D polyhedral meshes into 2D patches (flat-patterns) for manufacturing.

## Core Responsibility
The unfolding engine decomposes a 3D solid into one or more non-overlapping 2D islands. Each island represents a cluster of faces that have been "flattened" while preserving their connectivity.

## Key Files
- `clusterer.h`: Implements the greedy topological clustering algorithm.
- `flattener.h`: Handles the transformation of 3D faces to 2D space and overlap detection.
- `types.h`: Shared data structures for unfolding clusters.

## Algorithm: Greedy Topological Clustering
Instead of attempting to find a single perfect net (which is NP-hard), the engine:
1.  Starts with a set of seed faces.
2.  Iteratively attempts to join adjacent faces to an existing cluster.
3.  Validates that the addition of a face does not cause self-intersection in the 2D plane.
4.  Creates a new cluster (island) if a face cannot be joined without overlap.

## Topological Tagging
Output segments are tagged to distinguish between different manufacturing processes:
- `type: "fold"`: Edges that connect two faces within the same 2D island.
- `type: "cut"`: Boundary edges of the island.
