# `@jotcad/web` Library

Framework-independent rendering and geometry parsing utilities for JotCAD workspace environments.

## Directory Structure

* **`src/render/`**: Contains core Three.js visualization helpers and custom `.jot` text format decoders.
  * **`GeometryDecoder.js`**: Universal parser that converts custom `.jot` text geometry representations (`V`, `F`, `T`, `S`, `P` lines) into float lists and face groups.
  * **`Viewer3D.js`**: Universally loadable three.js canvas initialization manager, lighting controller, and rendering driver for STL and `.jot` shapes.

## Dependencies

* `three`: Standard WebGL 3D rendering library.
