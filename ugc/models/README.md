# JotCAD UGC Models Library

The `ugc/models/` directory houses reusable, parametric, and fluent CAD model recipes for standard hardware, controller accessories, fasteners, and electromechanical components.

## Directory Responsibilities

* **`controllers/`**: Game controller thumbsticks, replacement caps, D-pads, trigger extensions, and mounting adapters.

## Conventions

* Every model file is a `.jot` file defining an anonymous parametric operator: `(param1 = default1, ...) => body -> $out;`.
* The operator's path is inferred directly from the file path relative to `ugc/models/` (e.g. `controllers/moga_thumbstick.jot` is registered as `controllers/moga_thumbstick`).
