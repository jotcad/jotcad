# JotCAD Test Suite & Architecture: Build-Forward Plan

This document outlines the steps to resolve the test suite's stability and performance issues (RAM exhaustion, slow builds, parallel collisions) while maintaining the fundamental decentralized design of the VFS.

## Prerequisites
- Revert all local changes to the last clean commit.
- This plan starts from that "known good" architectural state.

---

## Phase 1: Robustness & Parallel Primitives
**Goal**: Fix known crashes and enable collision-free parallel execution.

1.  **Fix VFS Destructor SIGSEGV**:
    *   In `fs/cpp/vfs_core.cpp`, explicitly initialize `server_ptr_(nullptr)` in the `VFSNode` constructor. This ensures the destructor's `stop()` call is safe if `listen()` was never called.
2.  **Implement Isolated Sandboxing**:
    *   Update `MockVFS` (in `geo/test/test_base.h`) to accept a `test_name` string.
    *   Use this name to create unique storage directories (e.g., `.vfs_test_storage_<test_name>`).
    *   This allows multiple tests to run concurrently in the same process group without corrupting each other's filesystem state.

## Phase 2: The "Fat Library" Pattern (Correct Implementation)
**Goal**: Compile heavy CGAL logic once into a library, but maintain node-level registration.

1.  **Refactor Registration to be Instance-Based**:
    *   Update every operator's initialization function to accept a pointer to a specific VFS node:
        *   `static void box_init(fs::VFSNode* vfs)`
        *   `static void cut_init(fs::VFSNode* vfs)`
    *   Inside these functions, call `vfs->register_op(...)` directly.
2.  **Update Library Entry Point**:
    *   Update `void register_all_ops(fs::VFSNode* vfs)` in `geo/ops_library.cc`.
    *   This function now acts as an "Installer" that populates a specific node instance with all geometry operators.
3.  **Zero Global State**:
    *   Ensure NO `static` registries or maps are added to `VFSNode` or `Processor`. Capabilities must remain local to each instance.

## Phase 3: Lightweight Test Migration
**Goal**: Decouple tests from heavy headers to prevent RAM exhaustion.

1.  **Header Decoupling**: 
    *   Modify test files (e.g., `cut_test.cpp`) to stop including heavy headers like `cut_op.h`.
    *   They should only include `test_base.h` and see the signature for `register_all_ops(VFSNode*)`.
2.  **Explicit Capability Installation**:
    *   Each test main will look like this:
        ```cpp
        MockVFS vfs("my_test");
        register_all_ops(&vfs); // Explicitly "teach" this node geometry
        ```
3.  **Opaque Execution**:
    *   Use `vfs.read<Shape>(selector)` or a lightweight `Processor::execute(&vfs, selector)` helper. This triggers the handler already installed in that node's local toolbox.

## Phase 4: Parallel Rendering & Hash Stabilization
**Goal**: Transition to vertical camera and stable verification.

1.  **Configurable Rendering**:
    *   Update `Rasterizer::render_png` and `PngOp` to accept optional `ax`, `ay` camera angles.
    *   Set the system default to top-down vertical (`ax=0, ay=0`).
2.  **Concurrency Configuration**:
    *   Update `package.json` and `Makefiles` to cap concurrency at 4 (`-j4` and `--test-concurrency=4`).
3.  **Visual Baselines**:
    *   Run tests to capture stable SHA256 hashes for the new vertical perspective.
    *   Commit these golden hashes as the new ground truth.

---

## Success Criteria
- [ ] Build completes in < 30 seconds (down from minutes).
- [ ] `vfsA->register_op` does not affect `vfsB`.
- [ ] No SIGSEGV during test cleanup.
- [ ] All unit tests pass in parallel with concurrency 4.
