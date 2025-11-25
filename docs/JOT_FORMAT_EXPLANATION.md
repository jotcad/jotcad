---

### Understanding the JotCAD Object Transfer (JOT) Format

#### 1. What is the JOT Format?

The JotCAD Object Transfer (JOT) format is a specialized text-based serialization method used within the JotCAD ecosystem. Its primary purpose is to bundle a complete 3D `Shape` object, along with all its underlying geometric data (like vertices, faces, and edges), into a single, self-contained string. This makes it easy to save, load, or transfer complex 3D models as simple text.

#### 2. Why JOT?

Traditional 3D model formats often involve multiple files (e.g., a `.obj` file for geometry and a `.mtl` file for materials, or separate texture files). JOT simplifies this by:

- **Single-File Transfer:** Everything needed to reconstruct a `Shape` is in one string, ideal for network transfer (e.g., via `fetch` in a web app) or storage.
- **Self-Contained:** No external dependencies are needed once the JOT string is loaded.
- **Human-Readable (to an extent):** Being text-based, it's inspectable, unlike binary formats.
- **Asset Management:** It explicitly links the main `Shape` definition to its geometric assets using content-addressable hashes, ensuring data integrity and efficient caching.

#### 3. How JOT Works: Bundling Shapes and Assets

A JotCAD `Shape` object can be complex, often composed of transformations, tags, and references to actual 3D geometry. This geometry itself is stored as "assets" in the JotCAD system, identified by unique cryptographic hashes (TextIds) of their content.

The JOT format essentially creates a mini "archive" within a single string. This archive contains:

- **One main entry:** A JSON representation of the top-level `Shape` object. This JSON refers to its geometry by its TextId.
- **Multiple asset entries:** Each unique piece of geometry data referenced by the `Shape` (or its sub-shapes) is included as a separate entry, identified by its TextId.

#### 4. Structure of a JOT String

A JOT string is a sequence of "file entries," each representing a piece of data. Every entry starts with a newline character (`\n`), followed by a header, and then the content.

**General Entry Format:**

```
\n=<CONTENT_LENGTH> <FILENAME>\n<FILE_CONTENT>
```

- **`\n` (Entry Separator):** Each entry, including the very first one, begins with a newline. This helps parsers identify the start of a new data block.
- **`=<CONTENT_LENGTH>`:** An equals sign (`=`) followed by the exact byte length of the `<FILE_CONTENT>`. This allows the parser to know precisely how many bytes to read for the content.
- **`<FILENAME>`:** A logical path/name for the content. This is crucial for `fromJot` to understand what kind of data it's reading.
  - **Main Shape Data:** These filenames always start with `files/` (e.g., `files/my_model.json`). There should be exactly one such entry in a valid JOT string, containing the JSON definition of the `Shape`.
  - **Geometry Assets:** These filenames always start with `assets/text/` followed by the unique 64-character hash (TextId) of the geometry content (e.g., `assets/text/1d8aebd...`).
- **`\n` (Header-Content Splitter):** A newline separates the header line from the actual data.
- **`<FILE_CONTENT>`:** The raw data. For `files/` entries, it's a JSON string. For `assets/text/` entries, it's the raw text geometry data.

#### 5. Interacting with JOT: `toJot` and `fromJot`

- **`toJot(assets, shape, filename)`:** This asynchronous function takes a JotCAD `assets` object, a `Shape` object, and a `filename` (e.g., `files/my_model.json`). It traverses the `Shape`, collects all unique geometry assets, serializes the `Shape` to JSON, and then bundles everything into a single JOT-formatted string.
- **`fromJot(assets, jotText)`:** This asynchronous function takes a JotCAD `assets` object and a JOT-formatted string (`jotText`). It parses the `jotText`, identifies the main `Shape` definition (the `files/` entry), re-registers all geometry assets (`assets/text/` entries) with the `assets` system, and then reconstructs and returns the original `Shape` object.

#### 6. Example (Conceptual)

Imagine a simple red cube. Its JOT representation might look like this:

```
\n=123 files/red_cube.json\n{"geometry":"abc...xyz","tags":["color:red"]}\n=456 assets/text/abc...xyz\nV 8\nv 0 0 0\nv 1 0 0\n... (more cube geometry data) ...
```

When `fromJot` processes this:

1.  It reads the first entry: `files/red_cube.json` with its JSON content.
2.  It reads the second entry: `assets/text/abc...xyz` with its raw geometry content.
3.  It registers the geometry content (`V 8\nv 0 0 0...`) with the `assets` system under the hash `abc...xyz`.
4.  It then parses the JSON from `files/red_cube.json`, which references `geometry:"abc...xyz"`.
5.  Finally, it reconstructs the `Shape` object, linking it to the newly registered geometry asset.

This explanatory documentation should provide a clearer understanding for someone looking to use or implement the JOT format.
