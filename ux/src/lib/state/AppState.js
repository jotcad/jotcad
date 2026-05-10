import { createSignal, createMemo, batch } from 'solid-js';
import { createStore } from 'solid-js/store';
import { Worksheet } from '../vfs/Worksheet';
import { DYNAMIC_OPS_KEY, NODE_STATE_KEY, DESKTOP_STATE_KEY, DEFAULT_CODE } from './Config';

export const [openWindows, setOpenWindows] = createStore([]);
export const [desktopIcons, setDesktopIcons] = createStore([]);

export const [pointerCount, setPointerCount] = createSignal(0);
export const [isGesturing, setIsGesturing] = createSignal(false);
export const [gestureOwner, setGestureOwner] = createSignal(null); 

// Action to initialize state from storage
export const initAppState = () => {
  console.log('[AppState] Initializing from storage...');
  
  // 1. Load Windows
  const savedWindows = Worksheet.get(Worksheet.TIERS.WINDOWS);
  if (savedWindows && Array.isArray(savedWindows)) {
      setOpenWindows(savedWindows.map(w => ({
        ...w,
        type: w.type || 'editor',
        zIndex: w.zIndex || 10,
        isMaximized: w.isMaximized || false
      })));
  }

  // 2. Load Icons
  const savedIcons = Worksheet.get(Worksheet.TIERS.DESKTOP);
  if (savedIcons && Array.isArray(savedIcons)) {
      setDesktopIcons(savedIcons);
  } else {
      // Default system icons
      setDesktopIcons([
        { id: 'app:catalog', type: 'app', label: 'Catalog', x: 40, y: 40, target: 'catalog', icon: 'Database' },
        { id: 'app:mesh', type: 'app', label: 'Mesh Graph', x: 140, y: 40, target: 'mesh', icon: 'Network' },
        { id: 'app:settings', type: 'app', label: 'Settings', x: 40, y: 140, target: 'settings', icon: 'Settings' },
        { id: 'app:console', type: 'app', label: 'Console', x: 140, y: 140, target: 'console', icon: 'Terminal' },
        { id: 'action:new', type: 'action', label: 'New Op', x: 40, y: 240, target: 'new_op', icon: 'Plus' }
      ]);
  }
};

export const [graph, setGraph] = createSignal({});
export const [schemas, setSchemas] = createSignal({});
export const [pulse, setPulse] = createSignal([]);
export const [meshTopology, setMeshTopology] = createStore({ peers: [] });
export const [meshPositions, setMeshPositions] = createSignal({});
export const [logs, setLogs] = createSignal([]);
export const logActions = {
  add(msg, type = 'info') {
    setLogs(prev => [...prev, { msg: String(msg), type, t: Date.now() }].slice(-200));
  },
  clear() {
    setLogs([]);
  },
  initInterception() {
    if (typeof window === 'undefined' || window._LOGS_INTERCEPTED) return;
    window._LOGS_INTERCEPTED = true;

    const originalLog = console.log;
    const originalError = console.error;
    const originalWarn = console.warn;

    console.log = (...args) => {
      this.add(args.map(a => typeof a === 'object' ? JSON.stringify(a) : a).join(' '), 'info');
      originalLog.apply(console, args);
    };
    console.error = (...args) => {
      this.add(args.map(a => typeof a === 'object' ? JSON.stringify(a) : a).join(' '), 'error');
      originalError.apply(console, args);
    };
    console.warn = (...args) => {
      this.add(args.map(a => typeof a === 'object' ? JSON.stringify(a) : a).join(' '), 'warn');
      originalWarn.apply(console, args);
    };
  }
};
export const [isConnected, setIsConnected] = createSignal(false);
export const [discoveryStatus, setDiscoveryStatus] = createSignal('idle');
const [dynamicOpsRaw, setDynamicOpsRaw] = createSignal({});
export const dynamicOps = () => {
    const val = dynamicOpsRaw();
    console.log('[AppState] dynamicOps getter called. Current keys:', Object.keys(val));
    return val;
};
export const setDynamicOps = (val) => {
    console.log('[AppState] setDynamicOps setter called.');
    if (typeof val === 'function') {
        setDynamicOpsRaw(prev => {
            const next = val(prev);
            console.log('[AppState] setDynamicOps (function) next keys:', Object.keys(next));
            return next;
        });
    } else {
        console.log('[AppState] setDynamicOps (object) next keys:', Object.keys(val));
        setDynamicOpsRaw(val);
    }
};
export const [error, setError] = createSignal(null);

export const windowActions = {
  open(type, id, data = {}) {
    const existing = openWindows.find(w => w.id === id);
    if (existing) {
      this.raise(id);
      return;
    }

    const newWindow = {
      id,
      type,
      pos: data.pos || { x: 100, y: 100 },
      size: data.size || { width: 500, height: 600 },
      zIndex: Math.max(0, ...openWindows.map(w => w.zIndex), 10) + 1,
      minimized: false,
      isMaximized: false,
      ...data
    };

    setOpenWindows([...openWindows, newWindow]);
    this._save();
  },

  raise(id) {
    const maxZ = Math.max(10, ...openWindows.map(w => w.zIndex));
    const win = openWindows.find(w => w.id === id);
    if (win && win.zIndex === maxZ && openWindows.length > 1) return; 

    setOpenWindows(
      w => w.id === id,
      'zIndex',
      maxZ + 1
    );
    this._save();
  },

  close(id) {
    setOpenWindows(openWindows.filter(w => w.id !== id));
    this._save();
  },

  update(id, updates) {
    setOpenWindows(w => w.id === id, updates);
    this._save();
  },

  _save() {
    if (this._saveTimer) clearTimeout(this._saveTimer);
    this._saveTimer = setTimeout(() => {
        Worksheet.save(Worksheet.TIERS.WINDOWS, null, JSON.parse(JSON.stringify(openWindows)));
    }, 100);
  }
};

export const desktopActions = {
  updateIcon(id, updates) {
    setDesktopIcons(i => i.id === id, updates);
    this._save();
  },
  
  addIcon(icon) {
    setDesktopIcons([...desktopIcons, icon]);
    this._save();
  },

  _save() {
    if (this._saveTimer) clearTimeout(this._saveTimer);
    this._saveTimer = setTimeout(() => {
        Worksheet.save(Worksheet.TIERS.DESKTOP, null, JSON.parse(JSON.stringify(desktopIcons)));
    }, 100);
  }
};

// --- Compatibility Exports for JotNode/Blackboard ---
export const openEditors = openWindows;
export const setOpenEditors = setOpenWindows;
export const editorActions = {
  openOp(id, initialCode = DEFAULT_CODE, initialSchema = { arguments: [] }) {
    windowActions.open('editor', id, { code: initialCode, schema: initialSchema });
  },
  raiseOp: windowActions.raise.bind(windowActions),
  createNewOp() {
    const id = `user/op-${Math.random().toString(36).slice(2, 6)}`;
    this.openOp(id);
  },
  closeOp: windowActions.close.bind(windowActions),
  updateEditorState: windowActions.update.bind(windowActions),
  _saveAllEditors: windowActions._save.bind(windowActions)
};
