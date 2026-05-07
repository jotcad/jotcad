import {
  vfs,
  peerId,
  mesh,
  vfsActions
} from './vfs/MeshVFS.js';

import {
  openEditors,
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
  editorActions,
  DEFAULT_CODE
} from './state/AppState.js';

export { vfs, DEFAULT_CODE };

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

  // Editor Actions
  openOp: editorActions.openOp.bind(editorActions),
  raiseOp: editorActions.raiseOp.bind(editorActions),
  createNewOp: editorActions.createNewOp.bind(editorActions),
  closeOp: editorActions.closeOp.bind(editorActions),
  updateEditorState: editorActions.updateEditorState.bind(editorActions),

  // VFS/Mesh Actions
  discoverSchemas: vfsActions.discoverSchemas.bind(vfsActions),
  start: () => vfsActions.start(blackboard),
  publishDynamicOp: (path, schema, script, persist = true) => 
    vfsActions.publishDynamicOp(path, schema, script, persist, blackboard),
  
  stop: vfsActions.stop.bind(vfsActions),
  read: vfsActions.read.bind(vfsActions),
  write: vfsActions.write.bind(vfsActions),
  clearStorage: vfsActions.clearStorage.bind(vfsActions)
};

if (typeof window !== 'undefined') {
  window.blackboard = blackboard;
}
