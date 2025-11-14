# JotCAD Development Guide

This guide outlines the process for adding new geometric operations and features to JotCAD, focusing on the separation of concerns between the `geometry/` and `ops/` modules.

## 1. Adding a New Geometric Operation (e.g., `Orb`)

When adding a new geometric operation, you'll typically work across three main areas: `geometry/`, `ops/`, and `server/`.

### 1.1. `geometry/` Module (Core Geometric Logic)

This module handles the low-level C++ implementation and its JavaScript binding, as well as the high-level JavaScript wrapper that returns a `Shape` object.

#### 1.1.1. C++ Implementation (`geometry/*.h` and `geometry/wasm.cc`)

1.  **Create a C++ Header File (`geometry/your_operation.h`)**:
    *   Define the core C++ function for your geometric operation (e.g., `MakeOrb`).
    *   This function should take parameters relevant to the geometry (e.g., `angularBound`, `radiusBound`, `distanceBound` for `Orb`).
    *   It should interact with CGAL or other C++ libraries to perform the geometric computation.
    *   The function should return a `GeometryId` (an integer representing the geometry in the asset manager).
    *   **Example (`geometry/orb.h`):**
        ```cpp
        #pragma once

        #include "assets.h"
        #include "geometry.h"

        // Forward declarations for CGAL types if needed
        // ...

        GeometryId MakeOrb(Assets &assets,
                           double angular_bound,
                           double radius_bound,
                           double distance_bound);
        ```

2.  **Expose to JavaScript (`geometry/wasm.cc`)**:
    *   Include your new C++ header file.
    *   Create a binding function (e.g., `MakeOrbBinding`) that wraps your C++ function.
    *   This binding function will handle the conversion of JavaScript arguments to C++ types and vice-versa.
    *   Register this binding with `emnapi_create_function` so it's accessible from JavaScript.
    *   **Example (`geometry/wasm.cc` snippet):**
        ```cpp
        #include "orb.h" // Your new header

        // ... other includes ...

        napi_value MakeOrbBinding(napi_env env, napi_callback_info info) {
          // ... argument parsing ...
          // Call your C++ MakeOrb function
          GeometryId geometry_id = MakeOrb(assets, angular_bound, radius_bound, distance_bound);
          // ... return geometry_id ...
        }

        napi_value Init(napi_env env, napi_value exports) {
          // ... other registrations ...
          emnapi_create_function(env, nullptr, 0, MakeOrbBinding, nullptr, &orb_function);
          napi_set_named_property(env, exports, "MakeOrb", orb_function);
          return exports;
        }
        ```

3.  **Create a JavaScript Wrapper (`geometry/your_operation.js`)**:
    *   This file will be the primary interface for your geometric operation within the `geometry/` module.
    *   It should import the `cgal` object (which contains your C++ bindings) and `makeShape` from `geometry/shape.js`.
    *   It should take user-friendly parameters (e.g., `x, y, z, zag` for `Orb`).
    *   It should contain the logic to derive the low-level C++ parameters (e.g., `angularBound`, `radiusBound`, `distanceBound`) from the user-friendly ones.
    *   It calls the C++ binding (e.g., `cgal.MakeOrb`).
    *   **Crucially**: It must wrap the `GeometryId` returned by the C++ binding in a `Shape` object using `makeShape({ geometry: geometryId })`.
    *   Apply any necessary transformations (e.g., `scale`, `move`) to the `Shape` object to match the user's input.
    *   **Example (`geometry/orb.js`):**
        ```javascript
        import { cgal } from './getCgal.js';
        import { buildCorners, computeMiddle, computeScale } from './corners.js';
        import { makeShape } from './shape.js';

        const DEFAULT_ORB_ZAG = 1;
        const scaleVector = (s, v) => v.map((c) => s * c); // Or import from a utility

        export const Orb = (assets, x = 1, y = x, z = x, zag = DEFAULT_ORB_ZAG) => {
          const [c1, c2] = buildCorners(x, y, z);
          const scale = scaleVector(0.5, computeScale(c1, c2));
          const middle = computeMiddle(c1, c2);
          const radius = Math.max(...scale);
          const tolerance = zag / radius;

          const angularBound = 30;
          const radiusBound = tolerance;
          const distanceBound = tolerance;

          const geometryId = cgal.MakeOrb(assets, angularBound, radiusBound, distanceBound);

          return makeShape({ geometry: geometryId })
            .scale(scale[0], scale[1], scale[2])
            .move(middle[0], middle[1], middle[2]);
        };
        ```

4.  **Helper Functions**:
    *   If your operation requires new utility functions (like `computeMiddle`, `computeScale`, `scaleVector`), add them to existing relevant files (e.g., `geometry/corners.js` for corner-related logic, or directly in your operation's JS wrapper if very specific). Avoid creating new files for single, trivial utilities if they can fit naturally elsewhere.

5.  **Update Build Script (`geometry/build_native_node.sh`)**:
    *   If you added new C++ source files (`.cc`) or headers (`.h`), ensure they are included in the compilation command.
    *   Verify that all necessary include paths (especially for CGAL) are correctly specified.

### 1.2. `ops/` Module (Operational Interface)

This module defines how the operation is exposed to the user and how its arguments are parsed.

1.  **Create an Operational Wrapper (`ops/your_operation.js`)**:
    *   Import your JavaScript wrapper from `geometry/` (e.g., `import { Orb as op } from '../geometry/orb.js';`).
    *   Use `registerOp` to define your operation.
    *   **`spec`**: Define the expected arguments and their types. Use `interval` for numeric ranges, `options` for an object of named parameters, `array` for lists, etc. Refer to `ops/box.js` for examples of `interval` and `options`.
    *   **`code`**: This function receives the parsed arguments. It should extract the user-friendly parameters (e.g., `x, y, z, options`) and pass them to your `geometry/` wrapper.
    *   **Example (`ops/orb.js`):**
        ```javascript
        import { Orb as op } from '../geometry/orb.js';
        import { registerOp } from './op.js';

        export const Orb = registerOp({
          name: 'Orb',
          spec: [
            null,
            [
              'interval',
              'interval',
              'interval',
              [
                'options',
                {
                  zag: 'number',
                },
              ],
            ],
            'shape',
          ],
          code: (id, assets, input, x = 1, y = x, z = x, options = {}) => {
            const { zag } = options;
            return op(assets, x, y, z, zag);
          },
        });
        ```

### 1.3. `server/server.js` (API Exposure)

1.  **Import the New Operation**:
    *   Add an import statement for your new operation from `ops/`.
    *   **Example:** `import { Orb } from '../ops/orb.js';`

2.  **Register in `bindings` and `whitelist.functions`**:
    *   Add your operation to the `bindings` object, mapping its name to the imported function.
    *   Add its name to the `whitelist.functions` array to allow it to be executed by user code.
    *   **Example:**
        ```javascript
        const bindings = {
          // ... other bindings ...
          Orb, // Add your new operation here
        };

        const whitelist = {
          // ... other whitelisted functions ...
          functions: [
            // ... existing functions ...
            'Orb', // Add your new operation name here
          ],
        };
        ```

### 1.4. Testing

1.  **Create a Test File (`geometry/your_operation.test.js`)**:
    *   Write unit tests for your `geometry/your_operation.js` wrapper.
    *   Verify that it correctly calls the C++ binding and returns a `Shape` object with the expected properties.
    *   If applicable, generate PNGs to visually verify the output.
    *   **Example (`geometry/orb.test.js`):**
        ```javascript
        import { Orb } from './orb.js';
        import { assets } from './assets.js';
        import { renderPng } from './renderPng.js';

        test('Orb generates a sphere', async () => {
          const orb = Orb(assets, 10, 10, 10); // Example call
          expect(orb).toBeDefined();
          // Add more specific assertions about the shape's properties if possible
          await renderPng(orb, 'orb.test.png');
        });
        ```
2.  **End-to-End Server Test**:
    *   After all changes, restart the server.
    *   Make a request to the server that uses your new operation (e.g., `Arc2(15).Orb().png(...)`) and verify the output.

## 2. General Principles

*   **Separation of Concerns**:
    *   `geometry/`: Core geometric logic, C++ bindings, JavaScript wrappers that return `Shape` objects. Focus on mathematical correctness and efficient computation.
    *   `ops/`: User-facing interface, argument parsing, and translation to `geometry/` calls. Focus on user-friendliness and flexibility.
    *   `server/`: API exposure and sandboxing.
*   **`Shape` Object Return**: All JavaScript wrappers in `geometry/` that represent a geometric primitive should ultimately return a `Shape` object. This ensures consistency and allows chaining of transformations.
*   **Debugging**:
    *   For JavaScript: Use `console.log()` extensively.
    *   For C++: Add `printf` statements in `wasm.cc` or your C++ headers. Remember to rebuild the native addon after C++ changes.
*   **Rebuilding Native Addon**: After any changes to C++ files (`.h`, `.cc`), you *must* rebuild the native addon. Navigate to the `geometry/` directory and run `./build_native_node.sh`.

By following these guidelines, you can effectively extend JotCAD with new geometric capabilities.
