# JotCAD VFS: Alternative Mesh Infrastructures

This document compiles an architectural comparison of production-grade, open-source networking frameworks that could potentially replace or evolve the current custom Virtual File System (VFS) mesh.

---

## 1. Eclipse Zenoh (The Optimal Fit)

[Eclipse Zenoh](https://zenoh.io/) is a zero-overhead pub/sub/query protocol designed to unify data-in-motion (pub/sub), data-at-rest (queries/storages), and computations across edge, cloud, and constrained microcontrollers.

### Key Features:
*   **Query/Response & Pub/Sub**: Natively supports both routing patterns (queries match VFS `read_selector`/`read_cid` execution; pub/sub matches VFS `notify`/`subscribe`).
*   **Resource-Constrained Target**: Exposes a dedicated C client library, **`zenoh-pico`**, optimized specifically for microcontrollers (ESP32/ESP8266) with a minimal footprint (under 10 KB RAM).
*   **Decentralized Routing**: Peer-to-peer routers automatically manage path expressions and query forwarding.
*   **Transport Flexibility**: Operates over WebSockets, TCP, UDP, or raw serial links with zero-copy binary serialization.

---

## 2. NATS & NATS JetStream

[NATS](https://nats.io/) is a high-performance messaging system designed for cloud-to-edge communication, supporting Request-Reply messaging and distributed persistence (JetStream).

### Key Features:
*   **Request-Reply**: Fits the recursive VFS execution and resolver workflow natively.
*   **Leaf Nodes**: Edge nodes (such as the JotCAD local gateway) can act as local hubs that dynamically sync topics and storage with central clusters.
*   **JetStream Distributed Key-Value**: Handles data replication and state caching out-of-the-box.
*   **Constraint**: Microcontrollers are limited to client-only roles (using Arduino NATS clients); they cannot participate as storage-hosting peers.

---

## 3. MQTT + Central Broker

[MQTT](https://mqtt.org/) is the standard machine-to-machine (M2M)/IoT connectivity protocol.

### Key Features:
*   **Retained Messages**: Broker caches the last-known value of any state/topic (e.g. `sys/ota`). New subscribers immediately receive this cached value, resolving discovery without polling.
*   **Ultra-lightweight**: Minimal packet headers, built-in keep-alives, and simple client implementations.
*   **Constraint**: Utilizes a centralized broker model (like Mosquitto) rather than a decentralized peer-to-peer routing mesh. It lacks native content-addressing (CIDs), requiring hashing to be handled entirely in user space.

---

## 4. libp2p & IPFS

[libp2p](https://libp2p.io/) is the modular peer-to-peer networking stack powering Ethereum, IPFS, and Filecoin.

### Key Features:
*   **Pure P2P Routing**: Built-in NAT traversal, peer discovery, and Distributed Hash Tables (DHT) for decentralized query routing.
*   **Cryptographic Addressing**: Native cryptographic identity (Peer IDs) and content-addressing (CIDs) match the core invariants of the VFS spec.
*   **Constraint**: Highly complex stack with high CPU, cryptographic, and memory overhead. Running a native libp2p node on basic ESP32 or ESP8266 microcontrollers is not feasible.

---

## 5. Architectural Comparison Matrix

| Feature | Custom VFS | Eclipse Zenoh | NATS (JetStream) | MQTT | libp2p / IPFS |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Topology** | P2P + Gateway | P2P + Edge | Broker / Leaf | Central Broker | Pure P2P |
| **Microcontroller support** | Yes (custom) | Yes (`zenoh-pico`) | Yes (client-only) | Yes (native) | No (too heavy) |
| **Query/Response Routing** | Yes | Yes (Query) | Yes (Req-Reply) | No | Yes (DHT) |
| **Content Addressing (CIDs)**| Yes (native) | Yes (Key Expr) | No (manual) | No | Yes (native) |
| **State Caching** | No (metadata only)| Yes | Yes (JetStream) | Yes (Retained) | Yes (IPFS cache) |
| **NAT Traversal** | No | Yes | No | No | Yes |
| **Maintenance Cost** | High (custom) | Low (library) | Low (library) | Low (standard) | High (complex) |
