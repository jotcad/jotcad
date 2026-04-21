import { normalizeSelector } from './fs/src/vfs_core.js';
const selector = { path: 'jot/Box', parameters: { width: 10, height: 10, depth: 0 } };
const normalized = normalizeSelector(selector);
const json = JSON.stringify(normalized);
console.log('JS JSON: [' + json + ']');
console.log('JS JSON Bytes:', Array.from(new TextEncoder().encode(json)));
