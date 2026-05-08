import { createSignal, createMemo, batch } from 'solid-js';
import { createStore } from 'solid-js/store';

const storagePrefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
export const DYNAMIC_OPS_KEY = `${storagePrefix}_dynamic_ops`;
export const NODE_STATE_KEY = `${storagePrefix}_node_state`;

export const DEFAULT_CODE = `// Welcome to JotCAD
// Write your JOT script here
Box(50, 50, 20)
`;

const getInitialEditors = () => {
  const saved = localStorage.getItem(NODE_STATE_KEY);
  if (saved) {
    try {
      return JSON.parse(saved);
    } catch (e) {
      console.error('Failed to parse saved editor state:', e);
    }
  }
  return [];
};

export const [pointerCount, setPointerCount] = createSignal(0);
export const [isGesturing, setIsGesturing] = createSignal(false);
export const [gestureOwner, setGestureOwner] = createSignal(null); // 'blackboard' | 'node-id' | null
export const [openEditors, setOpenEditors] = createStore(getInitialEditors());
export const [graph, setGraph] = createSignal({});
export const [schemas, setSchemas] = createSignal({});
export const [pulse, setPulse] = createSignal([]);
export const [meshTopology, setMeshTopology] = createStore({ peers: [] });
export const [meshPositions, setMeshPositions] = createSignal({});
export const [logs, setLogs] = createSignal([]);
export const [isConnected, setIsConnected] = createSignal(false);
export const [discoveryStatus, setDiscoveryStatus] = createSignal('idle');
export const [dynamicOps, setDynamicOps] = createSignal({});
export const [error, setError] = createSignal(null);

export const editorActions = {
  openOp(id, initialCode = DEFAULT_CODE, initialSchema = { arguments: [] }) {
    const existing = openEditors.find(e => e.id === id);
    if (existing) {
      this.raiseOp(id);
      return;
    }

    const newEditor = {
      id,
      code: initialCode,
      schema: initialSchema,
      zIndex: openEditors.length + 10,
      minimized: false
    };

    setOpenEditors([...openEditors, newEditor]);
    this._saveAllEditors();
  },

  raiseOp(id) {
    const maxZ = Math.max(...openEditors.map(e => e.zIndex), 10);
    setOpenEditors(
      e => e.id === id,
      'zIndex',
      maxZ + 1
    );
    this._saveAllEditors();
  },

  createNewOp() {
    const id = `user/op-${Math.random().toString(36).slice(2, 6)}`;
    this.openOp(id);
  },

  closeOp(id) {
    setOpenEditors(openEditors.filter(e => e.id !== id));
    this._saveAllEditors();
  },

  updateEditorState(id, updates) {
    setOpenEditors(e => e.id === id, updates);
    if (updates.code || updates.schema || updates.minimized !== undefined) {
        this._saveAllEditors();
    }
  },

  _saveAllEditors() {
    const editors = JSON.parse(JSON.stringify(openEditors));
    localStorage.setItem(NODE_STATE_KEY, JSON.stringify(editors));
  }
};
