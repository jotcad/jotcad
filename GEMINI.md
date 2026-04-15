# JotCAD Project Context & Memories

## Architecture & Build

Refer to the root [README.md](./README.md) for core architecture and C++ rebuild
instructions.

## AI Operational Mandates

- **PROTOCOL INTEGRITY (CRITICAL)**: Do NOT "fix", "improve", or modify the core
  VFS Mesh protocols (`read`, `listen`, `register`, `notify`, `subscribe`,
  `spy`) or add new endpoints (like `version`, `watch`, `cid`) without explicit
  architectural discussion and permission from the user. The protocol is
  designed to be a pure, minimal, push/subscribe mesh. We have wasted
  significant time reverting over-engineered pull-based discovery hacks. Stick
  to the established Pub-Sub flows.
- **Directives & Execution**: AI may rebuild C++ (`./build.sh`) directly.
  However, AI must ALWAYS ask the user to start or restart the cluster/services
  (e.g., `npm start`, `./start_all.sh`, `fuser -k`). Always explain why a
  restart is needed.
- **Strict Case-Sensitivity**: Lookups are exact. PascalCase for Constructors
  (`Box`), camelCase for Operations (`.offset`).
- **Optimization Strategy**: Always prefer the "Tee Pattern" for side-effects
  like `.pdf()` using `metadata.aliases`.
- **Normalization**: Standardize all radial/apothem parameters to `diameter` for
  deterministic Content-IDs.
- **Formal VFS Links**: Use `vfs.link(src, tgt)` for aliases. Resolution is 
  STRICTLY metadata-driven (via `.meta` target). NEVER "guess" or sniff links 
  from raw data content (e.g., `vfs:/` text pointers are just data).
- **Agent Discovery**: The UX dynamically discovers schemas; ensure casing is
  handled correctly in the registration loop within `JotNode.jsx`.
