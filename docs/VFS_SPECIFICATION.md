# JotCAD Virtual File System (VFS) Specification

The JotCAD VFS is a distributed, content-addressable blackboard system designed for collaborative geometry processing across C++, Node.js, and Browser environments.

## 1. Content Addressing (CID)

Every artifact on the blackboard is addressed by a **Content-ID (CID)**. The CID must be deterministic across all platforms to allow for deduplication and global addressing.

### Hashing Standard
The CID is a SHA-256 hash of the concatenated **Path** and **Normalized Parameters**.

**Formula:** `SHA256(path + JSON.stringify(sort_keys(parameters)))`

**Implementation Rules:**
1.  **Parameters:** Must be a flat JSON object. Keys must be sorted alphabetically before stringification.
2.  **Stringification:** No spaces should be present in the JSON string (e.g., `{"a":1,"b":2}`).
3.  **Concatenation:** The path is prepended directly to the JSON string.

## 2. States & Lifecycle

A coordinate (`path` + `parameters`) can be in one of the following states:

- **PENDING:** Demand exists (someone called `tickle`), but no data is available.
- **PROVISIONING:** An agent has "leased" the coordinate and is currently computing the result.
- **AVAILABLE:** The result is computed and stored in the VFS.
- **LINKED:** An alias pointing to another coordinate.
- **SCHEMA:** A special state for path-level metadata (defined at `path@schema`).

## 3. High-Level Data API

To simplify agent development, the VFS provides data-aware methods that handle stream lifecycle and serialization automatically.

### `readData(path, parameters)`
Retrieves data and automatically deserializes it based on the following heuristics:
1.  **Schema Check:** If a `SCHEMA` exists for the path, it uses the schema's `type` (e.g., `object`, `mesh`) to determine parsing.
2.  **JSON Fallback:** If the text starts with `{` or `[`, it attempts `JSON.parse`.
3.  **Binary Detection:** If non-printable characters are detected, it returns a `Uint8Array`.
4.  **Text Fallback:** Returns a plain string.

### `writeData(path, parameters, data)`
Automatically serializes data into a stream for storage:
- **Object/Array:** Serialized via `JSON.stringify`.
- **String:** Encoded via `TextEncoder`.
- **Uint8Array/ArrayBuffer:** Written as raw bytes.

### `declare(path, schema)`
Formalizes the "form" of data for a path. 
- Stores the schema at the coordinate `${path}@schema`.
- Sets the state to `SCHEMA` (Priority 4).
- The `schema` object should follow JSON Schema conventions where possible.

## 4. Synchronization Protocol

### Initial Sync
When a new peer (e.g., the Browser UX) connects, it should perform a one-time pull of all active states from the Hub:
- `GET /vfs/states` -> returns `Array<{ cid, path, parameters, state }>`.

### Real-time Updates
Peers must subscribe to a Server-Sent Events (SSE) stream for live state changes:
- `GET /vfs/watch?peerId=<id>`

### Validation
Peers SHOULD validate that the `cid` received from a remote source matches their local calculation for the given `path` and `parameters`. Discrepancies indicate a hashing mismatch and must be treated as fatal errors in development.

## 4. REST API (The Hub)

The central Hub (usually Node.js) exposes the following endpoints:

- `POST /vfs/tickle`: Manifest demand for a coordinate.
- `POST /vfs/lease`: Secure a time-limited lock for computation.
- `PUT /vfs/write`: Upload a result (`AVAILABLE`).
- `POST /vfs/read`: Retrieve a result.
- `GET /vfs/states`: List all known coordinates.
- `GET /vfs/watch`: SSE stream for state changes.

## 5. Visualization & Previews

The JotCAD UX automatically generates 3D thumbnails for geometric artifacts stored on the blackboard.

### 5.1 Geometry Detection
A coordinate is flagged for visualization if:
1.  The `path` contains keywords: `mesh`, `triangle`, or `box`.
2.  The `SCHEMA` for the path declares `type: 'mesh'`.

### 5.2 Thumbnail Generation
When a compatible coordinate enters the `AVAILABLE` state:
1.  The UX fetches the data using `vfs.readData()`.
2.  The data is passed to an offscreen Three.js renderer.
3.  The renderer identifies the format:
    - **JOT Archive:** Parses bundled shapes and assets.
    - **Raw Inexact Geometry:** Parses `v/f/t/h` codes.
    - **Shape JSON:** Resolves hierarchy and `vfs:/` references.
4.  A 256x256 PNG is generated and cached in the browser's reactive state.
