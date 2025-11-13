import { createRequire } from 'node:module';
import { stat, readFile, writeFile, mkdir } from 'node:fs/promises'; // Import async fs functions
const require = createRequire(import.meta.url);

// import { nativeNode } from './native.node.cjs';
export const cgal = require('./native.node');

export const FS = {
  stat: (path) => stat(path),
  readFile: (path, options) => readFile(path, options),
  writeFile: (path, data, options) => writeFile(path, data, options),
  mkdir: (path) => mkdir(path, { recursive: true }),
};
