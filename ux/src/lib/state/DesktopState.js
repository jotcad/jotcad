import { createSignal } from 'solid-js';
import { createStore } from 'solid-js/store';
import { Worksheet } from '../vfs/Worksheet';
import { DEFAULT_CODE } from './Config';
import { JotRegistry } from '../vfs/JotRegistry';

export const [openWindows, setOpenWindows] = createStore([]);
export const [desktopIcons, setDesktopIcons] = createStore([]);

// Canonical System Icons
const DEFAULT_SYSTEM_ICONS = [
  { id: 'app:catalog', type: 'app', label: 'Catalog', x: 40, y: 40, target: 'catalog', icon: 'Database' },
  { id: 'app:mesh', type: 'app', label: 'Mesh Graph', x: 140, y: 40, target: 'mesh', icon: 'Network' },
  { id: 'app:settings', type: 'app', label: 'Settings', x: 40, y: 140, target: 'settings', icon: 'Settings' },
  { id: 'app:console', type: 'app', label: 'Console', x: 140, y: 140, target: 'console', icon: 'Terminal' },
  { id: 'action:sync', type: 'action', label: 'Sync Cloud', x: 40, y: 240, target: 'sync_cloud', icon: 'Cloud' },
  { id: 'action:new', type: 'action', label: 'New Op', x: 140, y: 240, target: 'new_op', icon: 'Plus' }
];

// Compatibility for JotNode/Blackboard
export const openEditors = openWindows;
export const setOpenEditors = setOpenWindows;

/**
 * Ensures system icons are always present in the icon list.
 */
const repairIcons = (icons) => {
    const list = Array.isArray(icons) ? [...icons] : [];
    for (const sysIcon of DEFAULT_SYSTEM_ICONS) {
        if (!list.find(i => i.id === sysIcon.id)) {
            list.push(sysIcon);
        }
    }
    return list;
};

export const initDesktopState = () => {
  console.log('[DesktopState] Initializing from storage...');
  
  // 1. Load Windows
  const savedWindows = Worksheet.get(Worksheet.TIERS.WINDOWS);
  if (savedWindows && Array.isArray(savedWindows)) {
      setOpenWindows(savedWindows.map(w => ({
        ...w,
        type: w.type || 'editor',
        zIndex: w.zIndex || 10,
        isMaximized: w.isMaximized || false,
        size: (!w.size || w.size.width <= 0) ? { width: 500, height: 600 } : w.size
      })));
  }

  // 2. Load Icons
  const savedIcons = Worksheet.get(Worksheet.TIERS.DESKTOP);
  const repaired = repairIcons(savedIcons);
  setDesktopIcons(repaired);
  
  // Immediately persist if we had to repair
  if (!savedIcons || savedIcons.length !== repaired.length) {
      Worksheet.save(Worksheet.TIERS.DESKTOP, null, repaired, false);
  }
};

export const windowActions = {
  open(type, id, data = {}) {
    const existing = openWindows.find(w => w.id === id);
    if (existing) {
      // Recovery: If window exists but is collapsed (0px), reset it
      if (!existing.size || existing.size.width <= 0) {
        this.update(id, { size: { width: 500, height: 600 } });
      }
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

  updateAndRename(oldId, newId, updates) {
    const list = openWindows.map(w => {
      if (w.id === oldId) return { ...w, ...updates, id: newId };
      return w;
    });
    setOpenWindows(list);
    this._save();
  },

  rename(oldId, newId) {
    this.updateAndRename(oldId, newId, {});
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

// Compatibility for JotNode
export const editorActions = {
  openOp(id, initialCode = null, initialSchema = null) {
    const ops = window.blackboard?.dynamicOps();
    const existing = ops ? ops[id] : null;

    // DRAFT-ON-OPEN: Target the NEXT version automatically
    // This ensures we start with the latest content but edit as a new draft.
    const targetId = JotRegistry.getNextVersionPath(id);

    const code = initialCode || existing?.script || DEFAULT_CODE;
    const schema = initialSchema || existing?.schema || { arguments: [] };

    console.log(`[DesktopState] openOp: Starting draft ${targetId} from ${id}`);
    
    windowActions.open('editor', targetId, { 
        code, 
        schema,
        opName: targetId, 
        label: targetId
    });
  },
  raiseOp: windowActions.raise.bind(windowActions),
  createNewOp(initialName = '') {
    const targetPath = initialName ? JotRegistry.normalizePath(initialName) : '';
    const winId = targetPath || `new-op-${Date.now()}`;
    
    windowActions.open('editor', winId, { 
        code: DEFAULT_CODE, 
        schema: { arguments: [] },
        opName: targetPath, 
        label: targetPath || 'New Operator'
    });
  },
  closeOp: windowActions.close.bind(windowActions),
  updateEditorState: windowActions.update.bind(windowActions),
  _saveAllEditors: windowActions._save.bind(windowActions)
};
