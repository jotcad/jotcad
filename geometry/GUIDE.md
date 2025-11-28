# Development Guide

This guide outlines how to add new operations and features to JotCAD.

## Adding a New Operation

To add a new geometric operation (e.g., `Orb`, `Box`, `Clip`, `Rule`),
follow these steps:

1.  **Define the C++ implementation (Header-Only):**

    - All core C++ logic for geometric operations within the `geometry/`
      module should generally reside in header files (e.g.,
      `geometry/your_op_name.h`) as header-only functions. This means all
      function definitions must be `inline` or within a class/struct
      definition.
    - This often involves:
      - Retrieving input `Geometry` objects from `Assets` using `Shape`'s
        `GeometryId()` and applying transformations (`Shape::GetTf()`).
      - Converting between `CGAL::Point_3<EK>` (used in `Geometry`) and
        `CGAL::Point_3<IK>` (used in `Mesh` and `PolygonalChain`). Use
        `CGAL::Cartesian_converter` for this.
      - Calling the relevant CGAL algorithms or custom C++ logic.
      - Converting resulting `Mesh` or `PolygonalChain` data back into
        `Geometry` objects. The `Geometry` class often has methods like
        `DecodeSurfaceMesh<K>(CGAL::Surface_mesh<...>)` for this.
      - Storing output `Geometry` objects (or other data like statistics,
        serialized to JSON) in `Assets` using `assets.TextId(Geometry)`.
    - The main C++ function should typically return a `GeometryId` for the
      primary output shape.

2.  **Expose the C++ function via Napi Binding (`geometry/wasm.cc`):**

    - Include your new C++ header (e.g., `#include "your_op_name.h"`).
    - Create a static `Napi::Value YourOpNameBinding(const Napi::CallbackInfo& info)`
      function.
      - This function is responsible for:
        - Asserting the correct number of arguments (`AssertArgCount`).
        - Parsing JavaScript inputs (e.g., `Assets` object, `Shape`
          objects, `Napi::Object` for options) and converting them to
          appropriate C++ types. Note that `Shape` objects will be
          passed as `Shape&` (non-const reference) to allow internal
          caching by `Shape::GeometryId()` and `Shape::GetTf()`.
        - Calling your C++ implementation function (e.g.,
          `geometry::YourOpName(...)`).
        - Converting the C++ function's return value (e.g.,
          `GeometryId`) back into a `Napi::Value` for JavaScript.
    - Register your binding in the `Init` function using
      `exports.Set(Napi::String::New(env, "YourOpName"),`
      `Napi::Function::New(env, YourOpNameBinding));`.

3.  **Create the JavaScript wrapper:**

    - Create a new JavaScript file (e.g., `geometry/your_op_name.js`) that
      imports `cgal` (the Napi binding) and `makeShape`.
    - Define a user-friendly JavaScript function (e.g.,
      `export const yourOpName = (...) => ...`).
    - This function should call `cgal.YourOpName(...)` with the provided
      arguments.
    - Typically, the `GeometryId` returned by `cgal.YourOpName` is then
      wrapped in a `Shape` object using `makeShape({ geometry: returnedGeometryId })`.
    - Export your new operation from `geometry/main.js` (e.g.,
      `export { yourOpName } from './your_op_name.js';`).

4.  **Update build scripts (if necessary):**

    - Since C++ implementations in `geometry/` are header-only, there are no
      new `.cc` files to compile directly.
    - However, if your C++ implementation uses headers from a new
      subdirectory (e.g., `geometry/rs/`), add the corresponding include
      path (e.g., `-I./rs`) to the `g++` or `em++` commands in
      `geometry/build_native_node.sh` and `geometry/build_wasm_node.sh`.

5.  **Add tests:**

    - Create a new test file (e.g., `geometry/your_op_name.test.js`) to
      verify the correctness of the geometric operation.
    - Use the `node:test` framework with `describe` and `it`.
    - Utilize `withTestAssets` for asset management and test isolation.
    - Create input `Shape` objects as needed (e.g., using `Point` or `Points`
      functions from `geometry/point.js`).
    - Assert that the returned `Shape` object from your JavaScript wrapper has
      a valid `geometry` (i.e., its `GeometryId` is not null or undefined).
      For visual operations, `renderPng` and `testPng` can be used.

6.  **Define the operation in `ops/` (if applicable):**

    - If this is a high-level operation intended for the JotCAD runtime,
      create a new JavaScript file (e.g., `ops/your_op_name.js`) that
      defines the operation's `spec` (schema for arguments) and `code` (how
      to execute the operation).
    - Use `registerOp` to make the operation available to the JotCAD runtime.

7.  **Update `server/server.js` (if applicable):**
    - If the operation is used by the server (e.g., called from `ops/`), add
      it to the `whitelist.functions` and `whitelist.methods` arrays to
      allow it to be executed in the sandboxed environment.

## Git Management

- **`.gitignore`:** Ensure that build artifacts, downloaded dependencies, and
  temporary files are properly ignored. Only `observed.*.png` files should
  be ignored among PNGs, as other PNGs are typically reference images for
  tests.
- **Commit Messages:** Use clear, concise, and descriptive commit messages.
