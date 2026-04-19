# JotCAD Project Context & Memories

## Architecture & Build

Refer to the root [README.md](./README.md) for core architecture and C++ rebuild
instructions. All C++ implementations must link against **`-lcrypto`**.

## AI Operational Mandates

- **PROTOCOL INTEGRITY (CRITICAL)**: Do NOT modify the core VFS Mesh protocols 
  (`read`, `listen`, `register`, `notify`, `subscribe`, `spy`) without explicit 
  discussion. The protocol is a pure, minimal, push/subscribe mesh. 
- **ACTOR FULFILLMENT MODEL**: Operators return `void` and MUST satisfy their 
  assigned address by calling `vfs->write<Shape>(fulfilling, out)`.
- **IMMUTABLE GEOMETRY**: Primitives and transformative operators MUST NOT 
  overwrite input CIDs. New geometry must be materialized via anonymous writes: 
  `vfs->write<Geometry>(res)`.
- **TOP-LEVEL NAMESPACES**: The `fs` namespace (VFS Mesh) and `jotcad` 
  namespace (Geometry Engine) are decoupled. Do NOT nest `fs` inside `jotcad`.
- **ATOMIC SELECTORS**: Standardize on `fs::Selector` as the atomic unit of 
  addressing. Avoid decomposed paths and parameters in the API.
- **MANDATORY RECIPES**: Chained operations (e.g., `On`, `At`) require explicit 
  `$in` placeholders in their recipes. No implicit magic fallbacks.
- **Directives & Execution**: AI may rebuild C++ (`./build.sh`) directly.
  AI must ALWAYS ask the user to start or restart the cluster/services
  (e.g., `npm start`, `./start_all.sh`).
- **Strict Case-Sensitivity**: Lookups are exact. PascalCase for Constructors
  (`Box`), camelCase for Operations (`.offset`).
- **Standardization**: All radial/apothem parameters MUST be normalized to 
  **`diameter`** before mesh commitment.
- **Formal VFS Links**: Use `vfs.link(src, tgt)` for aliases. Resolution is 
  STRICTLY metadata-driven.
