import { createSignal } from 'solid-js';
import {
  vfs,
  peerId,
  mesh,
  vfsActions
} from './vfs/MeshVFS.js';

import {
  openEditors,
  setOpenEditors,
  graph,
  schemas,
  pulse,
  pointerCount,
  isGesturing,
  setIsGesturing,
  gestureOwner,
  setGestureOwner,
  meshTopology,
  meshPositions,
  setMeshPositions,
  logs,
  isConnected,
  discoveryStatus,
  dynamicOps,
  error,
  setError,
  editorActions
} from './state/AppState.js';
import { NODE_STATE_KEY, DEFAULT_CODE } from './state/Config.js';

export { vfs, DEFAULT_CODE };

// --- Blackboard Event Bus ---
const [eventBus, setEventBus] = createSignal({ type: 'init', t: Date.now() });

export const blackboard = {
  vfs,
  peerId,
  graph,
  schemas,
  pulse,
  pointerCount,
  isGesturing,
  setIsGesturing,
  gestureOwner,
  setGestureOwner,
  meshTopology: () => meshTopology,
  meshPositions,
  setMeshPositions,
  logs,
  isConnected,
  discoveryStatus,
  dynamicOps,
  error,
  setError,
  openEditors: () => openEditors,
  setOpenEditors,
  NODE_STATE_KEY,

  // Event Bus
  events: () => eventBus(),
  emit: (type, data) => setEventBus({ type, data, t: Date.now() }),

  // Editor Actions
  openOp: (id, code, schema) => {
    editorActions.openOp(id, code, schema);
    blackboard.emit('editor:open', { id });
  },
  raiseOp: editorActions.raiseOp.bind(editorActions),
  createNewOp: editorActions.createNewOp.bind(editorActions),
  closeOp: (id) => {
    editorActions.closeOp(id);
    blackboard.emit('editor:close', { id });
  },
  updateEditorState: (id, updates) => {
    editorActions.updateEditorState(id, updates);
    if (updates.code || updates.schema) {
        blackboard.emit('op:update', { id, ...updates });
    }
  },

  // VFS/Mesh Actions
  discoverSchemas: vfsActions.discoverSchemas.bind(vfsActions),
  start: () => vfsActions.start(blackboard),
  publishDynamicOp: (path, schema, script, persist = true) => {
    vfsActions.publishDynamicOp(path, schema, script, persist, blackboard);
    if (persist) {
        blackboard.emit('op:publish', { path, schema, script });
    }
  },
  
  stop: vfsActions.stop.bind(vfsActions),
  read: vfsActions.read.bind(vfsActions),
  write: vfsActions.write.bind(vfsActions),
  clearStorage: vfsActions.clearStorage.bind(vfsActions)
};

if (typeof window !== 'undefined') {
  window.blackboard = blackboard;
}
