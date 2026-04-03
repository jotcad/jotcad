# JotCAD Interactive UX

A high-performance reactive graph visualizer for the JotCAD Distributed Blackboard.

## Tech Stack
- **Reactivity:** Solid.js (No Virtual DOM)
- **3D:** Three.js
- **Interactions:** Interact.js
- **Styling:** Tailwind CSS
- **Build:** Vite

## How to Run

### 1. Start the VFS Hub
```bash
npm run start:vfs
```

### 2. Start the Dispatcher (for C++ Agents)
In a separate terminal:
```bash
cd geo
# Ensure agents are built first: ./build.sh
node -e "import { VFS } from '../fs/src/index.js'; import { Dispatcher } from './src/dispatcher.js'; const vfs = new VFS({ id: 'dispatcher' }); const d = new Dispatcher(vfs, { hubUrl: 'http://localhost:9090/vfs', binDir: './bin' }); d.register('shape/box', 'box_agent'); d.register('shape/triangle', 'triangle_agent'); d.start();"
```

### 3. Start the UX App
In a third terminal:
```bash
cd ux
npm run dev
```

Visit `http://localhost:3000` to interact with the blackboard. You can type coordinates like `shape/box` or `shape/triangle` and see the C++ agents fulfill them in real-time.
