# JotCAD User-Generated Content (UGC) Engine

The `@jotcad/ugc` package is the headless core execution, evaluation, and custom fulfiller registry for User-Generated Content within the JotCAD spatial mesh. It is decoupled from presentation and UI logic.

## Directory Responsibilities

* **`ugc/package.json`**: Package configuration defining the `@jotcad/ugc` ES module exports.
* **`ugc/src/index.js`**: Unified `UGCEngine` entry orchestrator coordinating evaluation, fulfillers, and persistence.
* **`ugc/src/evaluator.js`**: Headless parsing, compilation, parameter binding, and AST evaluation logic.
* **`ugc/src/fulfiller.js`**: User-defined operator registry wrapping scripts as VFS providers and Zenoh mesh queryable endpoints.
* **`ugc/src/storage.js`**: Registry persistence manager saving/loading operator scripts and schemas.

## Usage Example

```javascript
import { UGCEngine } from '@jotcad/ugc';

const ugc = new UGCEngine(vfs, mesh);
await ugc.init(); // Restores persisted operators from registry

// 1. Register a user-defined operator fulfiller
await ugc.registerFulfiller({
  name: 'user/MyWheel',
  script: 'Box(width, width, 10) -> $out',
  schema: {
    arguments: [{ name: 'width', type: 'number', default: 20 }]
  }
});

// 2. Compile and evaluate a script calling the custom operator
const results = await ugc.evaluate('MyWheel(40) -> $out');
```
