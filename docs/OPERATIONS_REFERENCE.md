# JotCAD Operations Reference

This document provides a detailed reference for the built-in geometric operations available in the JotCAD DSL.

## 1. Path Construction

### `Link($in, tools, smooth=false, zag=0.0)`
Connects a series of points into an open path.

- **`$in`**: The primary shape (subject). Its vertices are used as the starting points.
- **`tools`**: A list of additional shapes. The vertices of these shapes are appended to the path in order.
- **`smooth`**: If `true`, applies Catmull-Rom interpolation to create a curved path.
- **`zag`**: Controls the resolution of the smooth curve (e.g., `0.1` results in 10 steps per segment).

### `Loop($in, tools, smooth=false, zag=0.0)`
Connects a series of points into a closed loop.

- **`$in`**, **`tools`**, **`smooth`**, **`zag`**: Same as `Link`, but the final point is automatically connected back to the first point to close the loop.

## 2. Point Collection Rules

Both `Link` and `Loop` use a recursive vertex collection strategy:
- If a shape contains **Geometry**, all of its vertices are harvested.
- If a shape has no geometry (like a container or a `Point()`), its **Origin** (0,0,0) in local space is used.
- All points are transformed into the world space of the operation before connection.

## 3. Primitives

### `Hexagon(...)`
Generates a regular 2D hexagon. The operator supports multiple dimensioning variants, which are automatically selected based on the provided arguments.

#### Common Arguments
- **`turns`**: Rotation in Tau (default `0.0`). `0.0` is flat-topped; `1/12` (30 deg) is pointy-topped.

#### Variants
| Variant | Argument | Description | VFS Path |
| :--- | :--- | :--- | :--- |
| **Radius** | `radius` | Center-to-corner distance. | `jot/Hexagon/radius` |
| **Diameter** | `diameter` | Corner-to-corner distance (diagonal). | `jot/Hexagon/diameter` |
| **Apothem** | `apothem` | Center-to-flat distance. | `jot/Hexagon/apothem` |
| **Edge-to-Edge** | `edgeToEdge`| Flat-to-flat distance. | `jot/Hexagon/edgeToEdge` |
| **Diagonal** | `diagonal` | Alias for diameter. | `jot/Hexagon/diagonal` |
| **Edge-Length** | `edgeLength`| Length of a single side. | `jot/Hexagon/edgeLength` |

#### Example
```js
// Flat-topped hexagon by radius
Hexagon(radius=10)

// Pointy-topped hexagon by edge-to-edge distance
Hexagon(edgeToEdge=20, turns=1/12)
```
