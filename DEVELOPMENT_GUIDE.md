# Development Guide

This guide outlines how to add new operations and features to JotCAD.

## Adding a New Operation

To add a new geometric operation (e.g., `Orb`, `Box`, `Clip`), follow these steps:

1.  **Define the C++ implementation:**
    *   Create a new header file (e.g., `geometry/orb.h`) for the C++ function.
    *   Implement the C++ logic in `geometry/wasm.cc` (or a new `.cc` file if complex).
    *   Ensure the C++ function is exposed to JavaScript via NAPI bindings in `geometry/wasm.cc`.

2.  **Create the JavaScript wrapper:**
    *   Create a new JavaScript file (e.g., `geometry/orb.js`) that imports the NAPI binding and provides a user-friendly JavaScript interface. This file should handle parameter validation and transformation into the format expected by the C++ binding.
    *   If the operation returns a geometric shape, ensure it's wrapped in a `Shape` object and applies necessary transformations (e.g., scaling, moving).

3.  **Define the operation in `ops/`:**
    *   Create a new JavaScript file (e.g., `ops/orb.js`) that defines the operation's `spec` (schema for arguments) and `code` (how to execute the operation).
    *   Use `registerOp` to make the operation available to the JotCAD runtime.

4.  **Update `server/server.js`:**
    *   Add the new operation to the `whitelist.functions` and `whitelist.methods` arrays to allow it to be executed in the sandboxed environment.

5.  **Add tests:**
    *   Create a new test file (e.g., `geometry/orb.test.js`) to verify the correctness of the geometric operation.
    *   For visual operations, use `renderPng` and `testPng` for visual regression testing.

6.  **Update build scripts (if necessary):**
    *   If new C++ files are added, update `geometry/build_native_node.sh` and `geometry/build_wasm_node.sh` to include them in the compilation process.

## Session Cleanup Logic

The session cleanup process is handled by `server/session.js`.

*   **Expiration:** Sessions are considered expired if their modification time (`mtime`) is older than `ONE_HOUR_IN_MS` (1 hour) or if their directory name ends with `.deleteme`.
*   **Non-recursive Deletion:** The cleanup process strictly avoids recursive deletion for the main session directory. It attempts to delete known subdirectories (`assets/`, `files/`, `result/`, `text/`) using `rm({ recursive: true, force: true })`.
*   **Directory Retention:** If a session directory contains unexpected files after cleaning known subdirectories, it is kept, and an `ENOTEMPTY` error is logged. This prevents accidental data loss.
*   **`--cleanup-and-exit`:** The `server.js` supports a command-line argument to perform a one-off session cleanup and then exit.

## Git Management

*   **`.gitignore`:** Ensure that build artifacts, downloaded dependencies, and temporary files are properly ignored. Only `observed.*.png` files should be ignored among PNGs, as other PNGs are typically reference images for tests.
*   **Commit Messages:** Use clear, concise, and descriptive commit messages.