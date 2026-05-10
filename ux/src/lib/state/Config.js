const storagePrefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';

export const DYNAMIC_OPS_KEY = `${storagePrefix}_dynamic_ops`;
export const NODE_STATE_KEY = `${storagePrefix}_node_state`;
export const DESKTOP_STATE_KEY = `${storagePrefix}_desktop_state`;

export const DEFAULT_CODE = `// Welcome to JotCAD
// Write your JOT script here
Box(50, 50, 20)
`;
