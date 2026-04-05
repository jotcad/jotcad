# JotCAD Interactive UX

A high-performance reactive graph visualizer for the JotCAD Distributed Blackboard.

## Tech Stack
- **Reactivity:** Solid.js (No Virtual DOM)
- **3D:** Three.js
- **Interactions:** Interact.js
- **Styling:** Tailwind CSS
- **Build:** Vite

## How to Run

### 1. Start the Full System
The easiest way to start the Hub, Dispatcher, and UX simultaneously is from the root directory:
```bash
npm start
```

### 2. Manual Start (Individual Components)
If you need to run components separately:

#### Start the VFS Hub
```bash
node fs/serve.js
```

#### Start the Dispatcher (for C++ Agents)
```bash
# In the root or geo directory
node orchestrator.js # (Or use the inline dispatcher script from orchestrator.js)
```

#### Start the UX App
```bash
cd ux
npm run dev -- --port 3030
```

Visit `http://localhost:3030` to interact with the blackboard. 

## CID Validation
The UX performs strict validation of Content-IDs (CIDs) received from the Hub. If a `CID Mismatch` error appears in the console, it indicates a hashing discrepancy between the Browser and the Hub.
