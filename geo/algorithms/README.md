# Geometry Algorithms (geo/algorithms)

Pure geometric kernels. These files contain the CGAL logic for creating or modifying mesh data.

- **Atomic Responsibility**: Perform a single geometric task (e.g., offset, box creation).
- **Invariants**: These files are "VFS-blind" and operate strictly on `Shape` or `Geometry` objects.
