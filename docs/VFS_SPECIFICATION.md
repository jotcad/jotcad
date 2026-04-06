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
1.  **Schema Check:** If a `SCHEMA` exists for the path, it uses the schema's `type`:
    - `object` or `array`: Returns `JSON.parse(text)`.
    - `mesh`: Returns plain `text` (essential for JOT/OBJ formats).
2.  **JSON Fallback:** If the text starts with `{` or `[`, it attempts `JSON.parse`.
3.  **Binary Detection:** If non-printable characters are detected, it returns a `Uint8Array`.
4.  **Text Fallback:** Returns a plain string.

## 4. Agent Dispatcher & Services

The JotCAD system uses a modular approach to manage geometry agents and path-level metadata.

### 4.1 Orchestrator
The `orchestrator.js` serves as the system manager, launching the VFS Hub, the Dispatcher Service, and the Interactive UX as independent processes.

### 4.2 Dispatcher Service
The `geo/src/dispatcher_service.js` is a dedicated Node.js service that:
1.  Registers C++ agents (e.g., `hexagon_agent`) to path prefixes (e.g., `shape/`).
2.  Declares path-level `SCHEMA` objects on the blackboard.
3.  Serves interactive Web Component scripts to the VFS for use in the UX.
4.  Watches for `PENDING` coordinates and spawns the appropriate C++ binaries.

## 5. Geometry Agents

Agents are C++ binaries that fulfill demand for specific paths on the blackboard.

### 5.1 Hexagon Agent (`shape/hexagon`)
Generates flat-topped hexagon geometry.
- **Parameters:**
    - `radius`: Distance from center to vertex (Default: 10).
    - `kerf`: Offset applied to the radius (Default: 0). Positive values expand the shape; negative values contract it.
- **Output:** Writes a `vfs:/geo/mesh` link and a Shape JSON to the requested coordinate.

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

The JotCAD UX provides rich visual feedback for geometric artifacts stored on the blackboard.

### 5.1 Geometry Detection
A coordinate is flagged for visualization if:
1.  The `path` contains keywords: `mesh`, `triangle`, or `box`.
2.  The `SCHEMA` for the path declares `type: 'mesh'`.

### 5.2 Thumbnail Chips (Collapsed View)
Coordinates identified as geometry are rendered as **Thumbnail Chips**:
- **Form:** 64x64px rounded squares with glass-morphism styling.
- **Preview:** A static 3D snapshot captured from the Z-forward perspective.
- **Status Badges:** Circular pips laid out contiguously starting from the **Top-Left** corner, wrapping **Clockwise**:
    1.  **Schema (Grey):** Formal data definition exists.
    2.  **State (White/Yellow/Blue):** Availability status (Available, Provisioning, or Pending).
    3.  **Thumbnail (Blue/Cyan):** Visualization status (Ready or Generating).
    4.  **Count (White):** Indicates multiple versions/parameters exist at this path.

### 5.3 Dynamic Viewing (Expanded View)
Double-clicking a Thumbnail Chip opens the **Interactive Viewport**:
- **Engine:** Three.js with `OrbitControls`.
- **Interaction:** Full rotation (Left-click), zoom (Scroll), and pan (Right-click).
- **Auto-Framing:** The camera automatically centers and scales to fit the geometry hierarchy upon loading.
- **Standard View:** Initial perspective is set to `(0,0,1)` looking toward the origin with `(0,1,0)` as the up vector.

## 6. Agent-Managed Custom UX

Agents can provide their own specialized interactive controls by serving **Web Components** via the VFS.

### 6.1 Schema Metadata
To register a custom UI, an agent must include the following fields in its path's `SCHEMA`:
- `ux`: A VFS URI pointing to the JavaScript source (e.g., `vfs:/ui/components/my-agent`).
- `componentName`: The registered HTML tag name of the Web Component (e.g., `my-agent-editor`).

### 6.2 Implementation (Web Components)
The provided JavaScript should register a standard Custom Element. The element receives its current state via attributes and communicates changes back to the blackboard via a **`param-change`** DOM event.

**Example Event:**
```javascript
this.dispatchEvent(new CustomEvent('param-change', {
    detail: { width: 50, height: 20 },
    bubbles: true,
    composed: true
}));
```

### 6.3 Dynamic Loading
When a node with a `ux` schema is expanded, the JotCAD UX:
1.  Downloads the script from the VFS.
2.  Creates a temporary Blob URL and `import()`s it.
3.  Instantiates the custom element and binds it to the blackboard coordinate.
