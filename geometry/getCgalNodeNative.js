import { createRequire } from 'node:module';
import nodeFS from 'node:fs';
import path from 'node:path';
const require = createRequire(import.meta.url);

// import { nativeNode } from './native.node.cjs';
export const cgal = require('./native.node');

export const FS = {
  stat: (path) => nodeFS.statSync(path),
  readFile: (path, options) => nodeFS.readFileSync(path, options),
  writeFile: (filePath, data, options) => {
    nodeFS.mkdirSync(path.dirname(filePath), { recursive: true });
    nodeFS.writeFileSync(filePath, data, options);
  },
  mkdir: (path) => nodeFS.mkdirSync(path, { recursive: true }),
};
