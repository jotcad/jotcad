# Development Guide

This guide outlines how to add new operations and features to JotCAD.

## 1. Core Concepts and Principles

This module facilitates the development of geometric operations by bridging
JavaScript (for high-level logic and user interaction) and C++ (for
performance-critical computations, especially those leveraging libraries like
CGAL). Napi bindings serve as the communication layer between these two
environments, enabling seamless data exchange and function calls.

## 2. Adding a New Operation (Workflow)

To add a new geometric operation (e.g., `Orb`, `Box`, `Clip`, `Rule`), follow
these steps:

1.  **Define the C++ implementation (Header-Only):**

    - All core C++ logic for geometric operations within the `geometry/` module
      should generally reside in header files (e.g., `geometry/your_op_name.h`)
      as header-only functions. This means all function definitions must be
      `inline` or within a class/struct definition. Such functions typically
      take `Assets& assets` and `Shape& shape` (and other arguments) as input.
    - This often involves:
      - Retrieving input `Geometry` objects from `Assets` using `Shape`'s
        `GeometryId()` and applying transformations (`Shape::GetTf()`), so that
        the geometry is in world coordinates.
      - Converting between `CGAL::Point_3<EK>` (used in `Geometry`) and
        `CGAL::Point_3<IK>` (used in `Mesh` and `PolygonalChain`). Use
        `CGAL::Cartesian_converter` for this.
      - Calling the relevant CGAL algorithms or custom C++ logic.
      - Storing output `Geometry` objects (or other data like statistics,
        serialized to JSON) in `Assets` using `assets.TextId(Geometry)`. The
        output `Geometry` should usually be transformed back into the original
        coordinate system of the input `Shape` by applying
        `shape.GetTf().inverse()`.
    - The main C++ function should typically return a `GeometryId` for the
      primary output shape.

2.  **Expose the C++ function via Napi Binding (`geometry/wasm.cc`):**

    - Include your new C++ header (e.g., `#include "your_op_name.h"`).
    - Create a static
      `Napi::Value YourOpNameBinding(const Napi::CallbackInfo& info)` function.
      - This function is responsible for:
        - Asserting the correct number of arguments (`AssertArgCount`).
        - Parsing JavaScript inputs (e.g., `Assets` object, `Shape` objects,
          `Napi::Object` for options) and converting them to appropriate C++
          types. Note that `Shape` objects will be passed as `Shape&` (non-const
          reference) to allow internal caching by `Shape::GeometryId()` and
          `Shape::GetTf()`.
        - Calling your C++ implementation function (e.g.,
          `geometry::YourOpName(...)`).
        - Converting the C++ function's return value (e.g., `GeometryId`) back
          into a `Napi::Value` for JavaScript.
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
    - Typically, the `GeometryId` returned by `cgal.YourOpName` is then wrapped
      in a `Shape` object using `makeShape({ geometry: returnedGeometryId })`.
    - Export your new operation from `geometry/main.js` (e.g.,
      `export { yourOpName } from './your_op_name.js';`).

4.  **Update build scripts (if necessary):**

    - Since C++ implementations in `geometry/` are header-only, there are no new
      `.cc` files to compile directly.
    - However, if your C++ implementation uses headers from a new subdirectory
      (e.g., `geometry/rs/`), add the corresponding include path (e.g.,
      `-I./rs`) to the `g++` command in `geometry/build_native_node.sh`. You
      should run `build_native_node.sh` from the `geometry/` directory. The
      `build_wasm_node.sh` script is used for WebAssembly compilation and is
      generally not required for Node.js based testing.

5.  **Add tests:**

    - Create a new test file (e.g., `geometry/your_op_name.test.js`) to verify
      the correctness of the geometric operation.
    - Use the `node:test` framework with `describe` and `it`.
    - Utilize `withTestAssets` for asset management and test isolation.
    - Create input `Shape` objects as needed (e.g., using `Point` or `Points`
      functions from `geometry/point.js`).
    - Assert that the returned `Shape` object from your JavaScript wrapper has a
      valid `geometry` (i.e., its `GeometryId` is not null or undefined). For
      visual operations, `renderPng` and `testPng` can be used.

6.  **Define the operation in `ops/` (if applicable):**

    - If this is a high-level operation intended for the JotCAD runtime, create
      a new JavaScript file (e.g., `ops/your_op_name.js`) that defines the
      operation's `spec` (schema for arguments) and `code` (how to execute the
      operation).
    - Use `registerOp` to make the operation available to the JotCAD runtime.

7.  **Update `server/server.js` (if applicable):**
    - If the operation is used by the server (e.g., called from `ops/`), add it
      to the `whitelist.functions` and `whitelist.methods` arrays to allow it to
      be executed in the sandboxed environment.

## 3. Testing

Testing is crucial for ensuring the correctness and stability of geometric
operations. This section outlines the various testing strategies and best
practices.

### 3.1 Unit Testing

- Create new test files (e.g., `geometry/your_op_name.test.js`) to verify the
  correctness of the geometric operation.
- Use the `node:test` framework with `describe` and `it`.
- Utilize `withTestAssets` for asset management and test isolation.
- Create input `Shape` objects as needed (e.g., using `Point` or `Points`
  functions from `geometry/point.js`).

### 3.2 Visual / Golden Image Testing

The testing strategy for visual operations relies on "golden image" testing. A
reference PNG image (the "golden" image) is stored in the repository. When a
test is run, it generates a new image, which is then compared pixel-by-pixel to
the golden image.

- If the generated image is different, the test will fail. The generated image
  is saved as `observed.*.png`. If the change is expected and correct, the
  golden image can be updated by copying the `observed.*.png` file over the
  original golden image file.
- **Using `testPng`:** The `testPng` helper requires the buffer of the rendered
  image to be passed as its second argument. The `renderPng` function
  conveniently returns this buffer. Always capture the return value from
  `renderPng` and pass it to `testPng`.
  ```javascript
  // Correct usage
  const buffer = await renderPng(myShape, outputPath);
  await testPng(outputPath, buffer);
  ```

### 3.3 Geometry Data Verification (`testJot`)

It's also important to test the serialized geometry using `testJot`. This
ensures that the underlying geometry data is correct and stable across changes,
not just the visual representation. Assertions should compare the generated text
representation of geometry against a known, expected string. Use multi-line
template literals for readability in these assertions.

## 4. Build Process and Environment Setup

This section details the necessary steps for building the native C++ components
and configuring the development environment.

### 4.1 Rebuilding Native Code

After making any changes to C++ files (`.h` or `.cc`) in the `geometry/`
directory, you **must** rebuild the native Node.js addon before running tests.
Failure to do so will result in your changes not being applied.

Run the following command from the `geometry/` directory:

```bash
./build_native_node.sh
```

### 4.2 Updating Build Scripts

- \*\*If your C++ implementation uses headers from a new subdirectory (e.g.,
  `geometry/rs/`), add the corresponding include path (e.g., `-I./rs`) to the
  `g++` command in `geometry/build_native_node.sh`. You should run
  `build_native_node.sh` from the `geometry/` directory. The
  `build_wasm_node.sh` script is used for WebAssembly compilation and is
  generally not required for Node.js based testing.

## 5. Debugging

Debugging for geometry operations often involves inspecting both the JavaScript
and C++ layers.

### 5.1 C++/JavaScript Interoperability Debugging

When developing or debugging geometric operations that bridge JavaScript and C++
(via Napi bindings), subtle issues can arise due to differences in data
representation, memory management, and parsing. Here are some key considerations
and best practices:

- **Strictness of Geometry Text Format:** The text-based geometry format used
  for serialization/deserialization between JavaScript and C++ is highly strict.
  Pay close attention to newlines, whitespace, and the exact syntax of commands
  (e.g., `v`, `p`, `s`, `T`, `t`, `f`, `h`). Any deviation can lead to C++
  parsing errors or incorrect data population.
  - **Example:** A JavaScript `Point` generating `p 0` might imply a specific
    point index, but if a newline is missing or extra whitespace is present, the
    C++ `Geometry::Decode` might fail to read it correctly.
- **Granular Logging for Data Flow:** When a geometric operation produces
  unexpected results (e.g., an empty geometry), use granular logging on both the
  C++ and JavaScript sides to trace the data flow at critical points.
  - **JavaScript Side:** Log the exact string being passed to `assets.textId()`
    or any Napi binding calls.
  - **C++ Side (within Napi binding functions and
    `Geometry::Decode`/`Encode`):**
    - Log the raw input string being received before decoding.
    - Log the state of relevant `Geometry` data members (e.g.,
      `vertices_.size()`, `points_.size()`, `segments_.size()`,
      `triangles_.size()`, `faces_.size()`) immediately after decoding, after
      transformations, and before critical processing steps.
    - Log the memory addresses of `Geometry` objects if you suspect copy/move
      semantic issues or object lifetime problems.
- **Robust C++ Decoding with Exceptions:** Implement robust error handling in
  C++ decoding functions. Instead of merely printing to `std::cout` or
  `std::cerr`, throw `std::runtime_error` or custom exceptions when invalid data
  or unexpected stream states are encountered. This ensures that errors are not
  silently ignored and can be propagated back to the JavaScript layer as
  `Napi::Error` exceptions for proper handling.
- **Verification of Reference Images and Assertions:** When a C++ geometry
  function's output changes (e.g., `grow` starts producing a correct box after a
  fix), ensure that corresponding JavaScript tests are updated. This often
  involves:
  - **Updating `textRepresentation` assertions:** The expected text output of
    the `Geometry` object should precisely match the new behavior. Use
    multi-line template literals for readability.
  - **Updating visual golden images:** If `testPng` fails due to an expected
    visual change, copy the `observed.*.png` to replace the outdated reference
    image.
- **Understanding `Geometry` Data Structure Population:** Be aware that a single
  concept (e.g., a "point") might be represented differently internally within
  the `Geometry` class (e.g., as an entry in `vertices_` and also referenced by
  `points_`). Ensure that the correct data members are being populated and
  checked by C++ algorithms.

### 5.2 General C++ Debugging

Debugging C++ code from Node.js can be challenging. Here are some tips:

- **Logging:** Use `std::cout` or `std::cerr` to print debug messages from your
  C++ code. These will be printed to the console when you run the tests.
- **Advanced Debugging:** For more complex issues, it's possible to attach a C++
  debugger (like GDB or LLDB) to the Node.js process. This allows you to set
  breakpoints and inspect variables in your C++ code. This is an advanced
  technique and requires a properly configured development environment.

## Troubleshooting and Best Practices

This section covers common issues and guidelines discovered during development.

### Git Management

- **`.gitignore`:** Ensure that build artifacts, downloaded dependencies, and
  temporary files are properly ignored. Only `observed.*.png` files should be
  ignored among PNGs, as other PNGs are typically reference images for tests.
- **Commit Messages:** Use clear, concise, and descriptive commit messages.

### Handling CGAL Polygon Orientation Errors

If you encounter a CGAL error like `The polygon has a wrong orientation`, it
means that a 2D boolean operation (often a union inside a function like
`footprint`) received polygons with inconsistent orientations.

This can be fixed by ensuring all 2D polygons are normalized to a consistent
orientation (e.g., counter-clockwise) before being passed to a union operation.
In the C++ code, you can fix this by checking and reversing the orientation of
polygons.

```cpp
// Example from footprint.h
Polygon_2 poly;
poly.push_back(p0);
poly.push_back(p1);
poly.push_back(p2);
// Normalize orientation before adding to the list for union
if (poly.is_clockwise_oriented()) {
  poly.reverse_orientation();
}
cgal_polygons.push_back(poly);
```

### API Design for Geometry Functions

JavaScript functions that wrap C++ geometry operations (like `cut.js` or
`footprint.js`) should be designed to accept the `assets` object as an explicit
argument. Avoid using `withAssets` inside these lower-level geometry wrappers.
This makes the functions synchronous (if the underlying C++ call is
synchronous), easier to test, and more composable.

**Good Pattern (`cut.js`):**

```javascript
export const cut = (assets, shape, tools) => {
  // ... call cgal.Cut(assets, shape, tools)
};
```
