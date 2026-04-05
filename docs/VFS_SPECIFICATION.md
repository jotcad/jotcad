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

## 3. Synchronization Protocol

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
