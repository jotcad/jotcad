import { createSignal } from 'solid-js';
import { createStore, reconcile } from 'solid-js/store';

const storagePrefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
export const NODE_STATE_KEY = `${storagePrefix}_node_state`;
export const DYNAMIC_OPS_KEY = `${storagePrefix}_dynamic_ops`;

export const DEFAULT_CODE = `
// JotCAD Expressions
Box(width, 10, 0)
`.trim();

const getInitialEditors = () => {
    try {
      const saved = localStorage.getItem(NODE_STATE_KEY);
      if (!saved) return [];
      const parsed = JSON.parse(saved);
      if (Array.isArray(parsed)) return parsed;
      if (parsed && typeof parsed === 'object' && parsed.opName) {
          return [{ ...parsed, id: `editor-${Math.random().toString(36).slice(2, 8)}` }];
      }
      return [];
    } catch (e) { return []; }
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

// Console redirection
const originalLog = console.log;
const originalWarn = console.warn;
const originalError = console.error;

const addLog = (type, args) => {
  const msg = args.map(a => 
    typeof a === 'object' ? JSON.stringify(a, (key, value) => 
      typeof value === 'bigint' ? value.toString() : value, 2) : String(a)
  ).join(' ');
  
  setLogs(prev => [{ type, msg, t: Date.now(), id: Math.random() }, ...prev].slice(0, 100));
};

console.log = (...args) => { originalLog(...args); addLog('log', args); };
console.warn = (...args) => { originalWarn(...args); addLog('warn', args); };
console.error = (...args) => { originalError(...args); addLog('error', args); };

export const editorActions = {
  openOp(path) {
    const existing = openEditors.find(e => e.opName === path);
    if (existing) {
        this.raiseOp(existing.id);
        return;
    }

    const op = dynamicOps()[path];
    const newState = {
        id: `editor-${Math.random().toString(36).slice(2, 8)}`,
        opName: path,
        code: op ? op.script : DEFAULT_CODE,
        args: op ? (op.schema.arguments || []).map(a => ({ name: a.name, type: a.type, testValue: a.default })) : 
             [{ name: 'width', type: 'jot:number', testValue: 20 }],
        pos: { x: 300 + (openEditors.length * 30), y: 300 + (openEditors.length * 30) }
    };
    
    setOpenEditors([...openEditors, newState]);
    this._saveAllEditors();
  },

  raiseOp(id) {
    const idx = openEditors.findIndex(e => e.id === id);
    if (idx !== -1 && idx !== openEditors.length - 1) {
      const editor = openEditors[idx];
      const next = [...openEditors.filter(e => e.id !== id), editor];
      setOpenEditors(reconcile(next));
    }
  },

  createNewOp(path) {
    const fullPath = path.startsWith('user/') ? path : `user/${path}`;
    const newState = {
        id: `editor-${Math.random().toString(36).slice(2, 8)}`,
        opName: fullPath,
        code: DEFAULT_CODE,
        args: [{ name: 'width', type: 'jot:number', testValue: 20 }],
        pos: { x: 300 + (openEditors.length * 30), y: 300 + (openEditors.length * 30) }
    };
    setOpenEditors([...openEditors, newState]);
    this._saveAllEditors();
  },

  closeOp(id) {
    setOpenEditors(openEditors.filter(e => e.id !== id));
    this._saveAllEditors();
  },

  updateEditorState(id, updates) {
    const idx = openEditors.findIndex(e => e.id === id);
    if (idx !== -1) {
        setOpenEditors(idx, updates);
        this._saveAllEditors();
    }
  },

  _saveAllEditors() {
    const editors = JSON.parse(JSON.stringify(openEditors));
    localStorage.setItem(NODE_STATE_KEY, JSON.stringify(editors));
    import('../vfs/RemoteStorageHandler').then(({ RemoteStorageHandler }) => {
        RemoteStorageHandler.pushLayout({ editors });
    }).catch(() => {});
  }
};
