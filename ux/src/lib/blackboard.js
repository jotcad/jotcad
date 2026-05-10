console.log('[Boot] blackboard.js loading...');
import { createSignal } from 'solid-js';
import {
  vfs,
  peerId,
  mesh,
  vfsActions
} from './vfs/VFSManager.js';

import {
  openWindows,
  setOpenWindows,
  desktopIcons,
  setDesktopIcons,
  windowActions,
  openEditors,
  setOpenEditors,
  editorActions
} from './state/DesktopState.js';

import {
  graph,
  schemas,
  pulse,
  meshTopology,
  meshPositions,
  setMeshPositions,
  isConnected,
  discoveryStatus,
  dynamicOps
} from './state/MeshState.js';

import {
  logs
} from './state/LogState.js';

import {
  pointerCount,
  setPointerCount,
  isGesturing,
  setIsGesturing,
  gestureOwner,
  setGestureOwner,
  error,
  setError
} from './state/SystemState.js';

import {
  NODE_STATE_KEY,
  DEFAULT_CODE
} from './state/Config.js';

console.log('[Boot] blackboard.js imports resolved.');

export { vfs, DEFAULT_CODE };

// --- Blackboard Event Bus ---
const [eventBus, setEventBus] = createSignal({ type: 'init', t: Date.now() });

export const blackboard = {
  vfs,
  mesh,
  peerId,
  
  // State Selectors (Signals)
  openEditors: () => openEditors,
  setOpenEditors,
  openWindows: () => openWindows,
  setOpenWindows,
  desktopIcons: () => desktopIcons,
  setDesktopIcons,
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

  // Actions
  ...editorActions,
  rename: windowActions.rename.bind(windowActions),
  discoverSchemas: vfsActions.discoverSchemas.bind(vfsActions),
  start: (bb) => vfsActions.start(bb || blackboard), 
  stop: vfsActions.stop.bind(vfsActions),
  publishDynamicOp: vfsActions.publishDynamicOp.bind(vfsActions),
  removeDynamicOp: vfsActions.removeDynamicOp.bind(vfsActions),
  read: vfsActions.read.bind(vfsActions),
  write: vfsActions.write.bind(vfsActions),
  clearStorage: vfsActions.clearStorage.bind(vfsActions),

  // Event Bus
  emit(type, detail = {}) {
    setEventBus({ type, detail, t: Date.now() });
  },
  on(type, callback) {
    // Basic effect-based listener would go here if needed
  },
  events: eventBus
};

if (typeof window !== 'undefined') {
  window.blackboard = blackboard;
}
